#include "track_timer/gnss/configure.hpp"
// For the DDC stream register alone. The configurator is otherwise transport-agnostic, and
// knows this one constant because the prefixed write form it tries is an I2C convention.
#include "track_timer/gnss/i2c_transport.hpp"
#include "track_timer/gnss/ubx.hpp"

#include <array>

namespace track_timer::gnss {
namespace {

void put_key(std::uint8_t* const output, std::size_t& offset, const std::uint32_t key) noexcept
{
    // Little-endian, like every multi-byte field in UBX.
    output[offset++] = static_cast<std::uint8_t>(key & 0xFFU);
    output[offset++] = static_cast<std::uint8_t>((key >> 8U) & 0xFFU);
    output[offset++] = static_cast<std::uint8_t>((key >> 16U) & 0xFFU);
    output[offset++] = static_cast<std::uint8_t>((key >> 24U) & 0xFFU);
}

// The value's width is not free: it is carried in the key itself, in bits 28-30. Writing a
// one-byte value for a two-byte key leaves the receiver reading the next key as part of this
// value, and it answers ACK-NAK rather than storing something wrong - which is the reason
// configuration here is acknowledged rather than assumed.
void put_u1(std::uint8_t* const output, std::size_t& offset, const std::uint32_t key,
            const std::uint8_t value) noexcept
{
    put_key(output, offset, key);
    output[offset++] = value;
}

void put_u2(std::uint8_t* const output, std::size_t& offset, const std::uint32_t key,
            const std::uint16_t value) noexcept
{
    put_key(output, offset, key);
    output[offset++] = static_cast<std::uint8_t>(value & 0xFFU);
    output[offset++] = static_cast<std::uint8_t>(value >> 8U);
}

[[nodiscard]] std::uint8_t layer_mask(const ConfigurationLayers& layers) noexcept
{
    std::uint8_t mask = 0;
    if (layers.ram) {
        mask |= kLayerRam;
    }
    if (layers.battery_backed) {
        mask |= kLayerBatteryBacked;
    }
    if (layers.flash) {
        mask |= kLayerFlash;
    }
    return mask;
}

// Looks through whatever the receiver has sent for an answer to the frame just written.
//
// Everything else in the stream is ignored rather than rejected. At this point the receiver
// is very likely still emitting NMEA, and it may already be emitting NAV-PVT, so the
// acknowledgement arrives surrounded by traffic that has nothing to do with it.
struct AcknowledgementSearch {
    UbxParser parser{};
    std::uint32_t frames{0};
    std::uint32_t acks{0};
    std::uint32_t naks{0};
    std::uint32_t bytes{0};

    void ingest(const std::uint8_t* const data, const std::size_t length) noexcept
    {
        bytes += static_cast<std::uint32_t>(length);
        for (std::size_t index = 0; index < length; ++index) {
            if (!parser.consume(data[index])) {
                continue;
            }
            ++frames;
            const auto message = parser.message();
            if (message.message_class != kClassAck) {
                continue;
            }
            // The payload names the message being answered. Checked, because the receiver
            // acknowledges everything it is sent and a stray ACK for something else would
            // otherwise read as success for this.
            const bool answers_valset = message.length >= 2 && message.payload != nullptr &&
                                        message.payload[0] == kClassCfg &&
                                        message.payload[1] == kIdCfgValset;
            if (!answers_valset) {
                continue;
            }
            if (message.message_id == kIdAckAck) {
                ++acks;
            }
            else if (message.message_id == kIdAckNak) {
                ++naks;
            }
        }
    }

    [[nodiscard]] bool settled() const noexcept { return acks > 0 || naks > 0; }
};

}  // namespace

std::size_t build_valset_payload(const GnssConfigurationRequest& request,
                                 std::uint8_t* const output, const std::size_t capacity) noexcept
{
    // Header, then five key-value pairs: 4 + 5 + 5 + 5 + 6 + 6.
    constexpr std::size_t needed = 31;
    if (output == nullptr || capacity < needed) {
        return 0;
    }
    if (request.measurement_period_ms == 0) {
        return 0;
    }

    std::size_t offset = 0;
    output[offset++] = 0x00;  // version: the plain form, not a transaction
    output[offset++] = layer_mask(request.layers);
    output[offset++] = 0x00;  // reserved
    output[offset++] = 0x00;

    // UBX on, so what this timer reads is emitted at all.
    put_u1(output, offset, kKeyI2cOutProtUbx, 1);
    // NMEA off, unless something still wants to read it.
    put_u1(output, offset, kKeyI2cOutProtNmea, request.silence_nmea ? 0 : 1);
    // NAV-PVT once per navigation solution. This is the message the whole chain is built on.
    put_u1(output, offset, kKeyMsgOutNavPvtI2c, 1);
    put_u2(output, offset, kKeyRateMeas, request.measurement_period_ms);
    // One navigation solution per measurement, so the measurement period is the fix rate.
    // Any other value makes the rate the pipeline was told to expect wrong, and its gap
    // detection measures loss against exactly that.
    put_u2(output, offset, kKeyRateNav, 1);

    return offset;
}

GnssConfigurationReport configure_receiver(GnssTransport& transport,
                                           const GnssConfigurationRequest& request,
                                           const WaitForReply wait,
                                           void* const wait_context) noexcept
{
    GnssConfigurationReport report{};

    std::array<std::uint8_t, kValsetPayloadCapacity> payload{};
    const auto payload_length = build_valset_payload(request, payload.data(), payload.size());
    if (payload_length == 0) {
        return report;
    }

    // The frame, and the same frame behind the DDC stream register. Both forms are built
    // once; which is sent is decided by whether the first is acknowledged.
    std::array<std::uint8_t, kValsetPayloadCapacity + 16> frame{};
    const auto frame_length =
        build_frame(kClassCfg, kIdCfgValset, payload.data(),
                    static_cast<std::uint16_t>(payload_length), frame.data() + 1, frame.size() - 1);
    if (frame_length == 0) {
        return report;
    }
    frame[0] = kStreamRegister;

    AcknowledgementSearch search{};
    std::array<std::uint8_t, 256> buffer{};

    for (int attempt = 0; attempt < 2; ++attempt) {
        const bool prefixed = attempt == 1;
        const auto* const data = prefixed ? frame.data() : frame.data() + 1;
        const auto length = prefixed ? frame_length + 1 : frame_length;

        if (!transport.write(data, length)) {
            // A transport that cannot write at all reports not_attempted, which is a
            // different thing from a bus that refused a frame it could have carried. The
            // first is a read-only transport; the second is wiring.
            report.outcome = transport.healthy() ? ConfigurationOutcome::not_attempted
                                                 : ConfigurationOutcome::write_failed;
            return report;
        }

        for (std::uint32_t poll = 0; poll < kAcknowledgementPolls; ++poll) {
            if (wait != nullptr) {
                wait(wait_context);
            }
            const auto read = transport.read(buffer.data(), buffer.size());
            if (read > 0) {
                search.ingest(buffer.data(), read);
            }
            if (search.settled()) {
                break;
            }
        }

        if (search.settled()) {
            report.stream_prefixed = prefixed;
            break;
        }
    }

    report.frames_seen = search.frames;
    report.acks = search.acks;
    report.naks = search.naks;
    report.bytes_read = search.bytes;
    if (search.naks > 0) {
        // A refusal outranks an acknowledgement: if any part of the frame was refused the
        // configuration is not the one that was asked for, and reporting success because
        // something else was accepted would be the dishonest half of the answer.
        report.outcome = ConfigurationOutcome::rejected;
    }
    else if (search.acks > 0) {
        report.outcome = ConfigurationOutcome::acknowledged;
    }
    else {
        report.outcome = ConfigurationOutcome::unacknowledged;
    }
    return report;
}

}  // namespace track_timer::gnss
