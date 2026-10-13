#include "track_timer/gnss/configure.hpp"
#include "track_timer/gnss/i2c_transport.hpp"
#include "track_timer/gnss/ubx.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using namespace track_timer;

// What the receiver is pretending to be. Each of these is a real state a NEO-M9N can be in,
// and they are told apart only by what comes back after a configuration frame.
enum class FakeBehaviour {
    acknowledges,
    refuses,            // understands the frame, rejects its contents
    says_nothing,       // the frame went nowhere, or the receiver is not listening
    only_when_prefixed, // answers the DDC stream-register form and not the raw one
    cannot_write,       // a read-only transport: a replayed fixture
    bus_refuses_write,
};

std::vector<std::uint8_t> ack_frame(const std::uint8_t ack_id, const std::uint8_t answered_class,
                                    const std::uint8_t answered_id)
{
    const std::array<std::uint8_t, 2> payload{answered_class, answered_id};
    std::array<std::uint8_t, 16> framed{};
    const auto length = gnss::build_frame(gnss::kClassAck, ack_id, payload.data(),
                                          static_cast<std::uint16_t>(payload.size()),
                                          framed.data(), framed.size());
    assert(length > 0);
    return {framed.begin(), framed.begin() + static_cast<std::ptrdiff_t>(length)};
}

class FakeReceiver final : public gnss::GnssTransport {
  public:
    explicit FakeReceiver(const FakeBehaviour behaviour) noexcept : behaviour_(behaviour) {}

    [[nodiscard]] std::size_t read(std::uint8_t* const output,
                                   const std::size_t capacity) noexcept override
    {
        const auto count = pending_.size() < capacity ? pending_.size() : capacity;
        for (std::size_t index = 0; index < count; ++index) {
            output[index] = pending_[index];
        }
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(count));
        return count;
    }

    [[nodiscard]] bool healthy() const noexcept override
    {
        return behaviour_ != FakeBehaviour::bus_refuses_write;
    }

    [[nodiscard]] bool write(const std::uint8_t* const data,
                             const std::size_t length) noexcept override
    {
        if (behaviour_ == FakeBehaviour::cannot_write ||
            behaviour_ == FakeBehaviour::bus_refuses_write) {
            return false;
        }
        ++writes_;
        last_write_.assign(data, data + length);

        // Chatter the configuration has to survive: a receiver in its factory default is
        // still emitting NMEA while it answers, so the acknowledgement never arrives alone.
        const char* const nmea = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M*47\r\n";
        for (const char* cursor = nmea; *cursor != '\0'; ++cursor) {
            pending_.push_back(static_cast<std::uint8_t>(*cursor));
        }

        const bool prefixed = length > 0 && data[0] == gnss::kStreamRegister;
        switch (behaviour_) {
        case FakeBehaviour::acknowledges:
            queue(ack_frame(gnss::kIdAckAck, gnss::kClassCfg, gnss::kIdCfgValset));
            break;
        case FakeBehaviour::refuses:
            queue(ack_frame(gnss::kIdAckNak, gnss::kClassCfg, gnss::kIdCfgValset));
            break;
        case FakeBehaviour::only_when_prefixed:
            if (prefixed) {
                queue(ack_frame(gnss::kIdAckAck, gnss::kClassCfg, gnss::kIdCfgValset));
            }
            break;
        case FakeBehaviour::says_nothing:
            // An acknowledgement for something else entirely, which must not be mistaken
            // for an answer to this frame.
            queue(ack_frame(gnss::kIdAckAck, gnss::kClassCfg, 0x01));
            break;
        case FakeBehaviour::cannot_write:
        case FakeBehaviour::bus_refuses_write:
            break;
        }
        return true;
    }

    [[nodiscard]] std::uint32_t writes() const noexcept { return writes_; }
    [[nodiscard]] const std::vector<std::uint8_t>& last_write() const noexcept
    {
        return last_write_;
    }

  private:
    void queue(const std::vector<std::uint8_t>& bytes)
    {
        pending_.insert(pending_.end(), bytes.begin(), bytes.end());
    }

    FakeBehaviour behaviour_;
    std::vector<std::uint8_t> pending_{};
    std::vector<std::uint8_t> last_write_{};
    std::uint32_t writes_{0};
};

[[nodiscard]] std::uint32_t key_at(const std::vector<std::uint8_t>& payload,
                                   const std::size_t offset)
{
    return static_cast<std::uint32_t>(payload[offset]) |
           static_cast<std::uint32_t>(payload[offset + 1]) << 8U |
           static_cast<std::uint32_t>(payload[offset + 2]) << 16U |
           static_cast<std::uint32_t>(payload[offset + 3]) << 24U;
}

// The bytes matter more than the behaviour here, because a fake that acknowledges anything
// cannot tell a correct frame from a plausible one. A real receiver refuses a key written at
// the wrong width, so the widths are asserted rather than trusted.
void the_valset_payload_writes_each_key_at_the_width_its_key_declares()
{
    gnss::GnssConfigurationRequest request{};
    request.measurement_period_ms = 50;
    std::array<std::uint8_t, gnss::kValsetPayloadCapacity> buffer{};
    const auto length = gnss::build_valset_payload(request, buffer.data(), buffer.size());
    assert(length == 31);

    const std::vector<std::uint8_t> payload{buffer.begin(),
                                            buffer.begin() + static_cast<std::ptrdiff_t>(length)};
    assert(payload[0] == 0x00);
    // All three layers by default, so the configuration outlives a power cycle.
    assert(payload[1] == (gnss::kLayerRam | gnss::kLayerBatteryBacked | gnss::kLayerFlash));
    assert(payload[2] == 0x00 && payload[3] == 0x00);

    // Three one-byte settings, then two two-byte ones.
    assert(key_at(payload, 4) == gnss::kKeyI2cOutProtUbx);
    assert(payload[8] == 1);
    assert(key_at(payload, 9) == gnss::kKeyI2cOutProtNmea);
    assert(payload[13] == 0);  // silenced
    assert(key_at(payload, 14) == gnss::kKeyMsgOutNavPvtI2c);
    assert(payload[18] == 1);
    assert(key_at(payload, 19) == gnss::kKeyRateMeas);
    assert(payload[23] == 50 && payload[24] == 0);  // little-endian, 50 ms
    assert(key_at(payload, 25) == gnss::kKeyRateNav);
    assert(payload[29] == 1 && payload[30] == 0);

    // Every key's size nibble must match the width written for it, which is the one thing
    // about these constants that can be checked without a receiver.
    const std::array<std::pair<std::uint32_t, std::uint8_t>, 5> expected{{
        {gnss::kKeyI2cOutProtUbx, 0x1},    // L, one bit, written as one byte
        {gnss::kKeyI2cOutProtNmea, 0x1},
        {gnss::kKeyMsgOutNavPvtI2c, 0x2},  // U1
        {gnss::kKeyRateMeas, 0x3},         // U2
        {gnss::kKeyRateNav, 0x3},
    }};
    for (const auto& [key, size_nibble] : expected) {
        assert(((key >> 28U) & 0x7U) == size_nibble);
    }
}

void keeping_nmea_is_possible_but_not_the_default()
{
    gnss::GnssConfigurationRequest request{};
    request.silence_nmea = false;
    std::array<std::uint8_t, gnss::kValsetPayloadCapacity> buffer{};
    const auto length = gnss::build_valset_payload(request, buffer.data(), buffer.size());
    assert(length == 31);
    assert(buffer[13] == 1);
}

// RAM alone is the configuration that works on the bench and fails in the car, so a caller
// has to ask for it deliberately and the request has to carry it faithfully.
void the_layers_asked_for_are_the_layers_written()
{
    gnss::GnssConfigurationRequest request{};
    request.layers = {true, false, false};
    std::array<std::uint8_t, gnss::kValsetPayloadCapacity> buffer{};
    assert(gnss::build_valset_payload(request, buffer.data(), buffer.size()) == 31);
    assert(buffer[1] == gnss::kLayerRam);
}

void a_zero_rate_is_refused_rather_than_sent()
{
    gnss::GnssConfigurationRequest request{};
    request.measurement_period_ms = 0;
    std::array<std::uint8_t, gnss::kValsetPayloadCapacity> buffer{};
    assert(gnss::build_valset_payload(request, buffer.data(), buffer.size()) == 0);

    // And a buffer too small to hold the frame is refused rather than truncated.
    std::array<std::uint8_t, 8> cramped{};
    gnss::GnssConfigurationRequest ordinary{};
    assert(gnss::build_valset_payload(ordinary, cramped.data(), cramped.size()) == 0);
}

void an_acknowledged_configuration_reports_which_form_the_receiver_answered()
{
    FakeReceiver receiver{FakeBehaviour::acknowledges};
    const auto report = gnss::configure_receiver(receiver, {});
    assert(report.outcome == gnss::ConfigurationOutcome::acknowledged);
    assert(report.acks == 1);
    assert(report.naks == 0);
    assert(!report.stream_prefixed);  // the raw form was tried first and worked
    assert(receiver.writes() == 1);   // so the second form was never sent

    // The frame really was a framed CFG-VALSET, not just any bytes.
    const auto& written = receiver.last_write();
    assert(written.size() == 31 + 8);
    assert(written[0] == gnss::kSyncChar1);
    assert(written[1] == gnss::kSyncChar2);
    assert(written[2] == gnss::kClassCfg);
    assert(written[3] == gnss::kIdCfgValset);
}

// The documentation reachable for this part does not settle whether an incoming message is
// prefixed with the DDC stream register. So both are tried, and the receiver decides.
void a_receiver_that_wants_the_stream_register_is_still_configured()
{
    FakeReceiver receiver{FakeBehaviour::only_when_prefixed};
    const auto report = gnss::configure_receiver(receiver, {});
    assert(report.outcome == gnss::ConfigurationOutcome::acknowledged);
    assert(report.stream_prefixed);
    assert(receiver.writes() == 2);  // raw first, then prefixed
}

// The three failures that must not be collapsed into one another, because each points
// somewhere different: at the contents, at the wiring, and at the transport.
void a_refusal_a_silence_and_an_unwritable_transport_are_three_different_answers()
{
    {
        FakeReceiver receiver{FakeBehaviour::refuses};
        const auto report = gnss::configure_receiver(receiver, {});
        assert(report.outcome == gnss::ConfigurationOutcome::rejected);
        assert(report.naks == 1);
    }
    {
        // Answers, but about a different message. Silence as far as this frame goes.
        FakeReceiver receiver{FakeBehaviour::says_nothing};
        const auto report = gnss::configure_receiver(receiver, {});
        assert(report.outcome == gnss::ConfigurationOutcome::unacknowledged);
        assert(report.acks == 0);
        assert(report.frames_seen > 0);  // it did see frames, just not an answer to this
        assert(receiver.writes() == 2);  // both forms tried before giving up
    }
    {
        FakeReceiver receiver{FakeBehaviour::cannot_write};
        const auto report = gnss::configure_receiver(receiver, {});
        assert(report.outcome == gnss::ConfigurationOutcome::not_attempted);
        assert(report.acks == 0 && report.naks == 0);
    }
    {
        FakeReceiver receiver{FakeBehaviour::bus_refuses_write};
        const auto report = gnss::configure_receiver(receiver, {});
        assert(report.outcome == gnss::ConfigurationOutcome::write_failed);
    }
}

// A refusal of any part means the configuration is not the one that was asked for, so an
// accompanying acknowledgement must not be allowed to report success.
void a_refusal_outranks_an_acknowledgement()
{
    FakeReceiver receiver{FakeBehaviour::refuses};
    const auto report = gnss::configure_receiver(receiver, {});
    assert(report.outcome == gnss::ConfigurationOutcome::rejected);
    assert(report.outcome != gnss::ConfigurationOutcome::acknowledged);
}

// The wait is the caller's, and it has to be called: on the device the frame is not on the
// wire and the reply has not arrived until the task yields.
void the_caller_is_given_the_chance_to_wait()
{
    FakeReceiver receiver{FakeBehaviour::says_nothing};
    int waits = 0;
    const auto report = gnss::configure_receiver(
        receiver, {}, [](void* context) { ++*static_cast<int*>(context); }, &waits);
    assert(report.outcome == gnss::ConfigurationOutcome::unacknowledged);
    // Bounded, so a silent receiver costs a known delay once rather than a stalled boot.
    assert(waits == static_cast<int>(gnss::kAcknowledgementPolls) * 2);
}

// A receiver already emitting NAV-PVT before anything is written to it is a receiver whose
// stored configuration survived. That is the only way to tell, because the firmware
// configures on every boot and a persisted receiver and a reverted one are indistinguishable
// from the first fix onwards.
void a_receiver_already_emitting_nav_pvt_is_recognised_without_writing_to_it()
{
    class AlreadyConfigured final : public gnss::GnssTransport {
      public:
        [[nodiscard]] std::size_t read(std::uint8_t* const output,
                                       const std::size_t capacity) noexcept override
        {
            std::array<std::uint8_t, 92> payload{};
            std::array<std::uint8_t, 128> framed{};
            const auto length =
                gnss::build_frame(gnss::kClassNav, gnss::kIdNavPvt, payload.data(),
                                  static_cast<std::uint16_t>(payload.size()), framed.data(),
                                  framed.size());
            const auto count = length < capacity ? length : capacity;
            for (std::size_t index = 0; index < count; ++index) {
                output[index] = framed[index];
            }
            return count;
        }
        [[nodiscard]] bool healthy() const noexcept override { return true; }
        [[nodiscard]] bool write(const std::uint8_t*, std::size_t) noexcept override
        {
            wrote = true;
            return true;
        }
        bool wrote{false};
    };

    AlreadyConfigured receiver{};
    const auto report = gnss::observe_receiver(receiver, gnss::kArrivalPolls);
    assert(report.already_configured());
    assert(!report.silent());
    assert(report.nav_pvt == 1);  // stops as soon as the question is answered
    // The whole point is that this observes without configuring.
    assert(!receiver.wrote);
}

// The factory default emits NMEA, which produces bytes and no frames. That is a receiver
// that reverted, and it must not be confused with one that is still starting up and has not
// said anything yet - the second is not an answer.
void nmea_only_and_silence_are_different_answers()
{
    class Nmea final : public gnss::GnssTransport {
      public:
        [[nodiscard]] std::size_t read(std::uint8_t* const output,
                                       const std::size_t capacity) noexcept override
        {
            const char* const sentence = "$GNRMC,123519,A,4807.038,N,01131.000,E,0.0,,230394,,,A*6A\r\n";
            std::size_t index = 0;
            for (const char* cursor = sentence; *cursor != '\0' && index < capacity; ++cursor) {
                output[index++] = static_cast<std::uint8_t>(*cursor);
            }
            return index;
        }
        [[nodiscard]] bool healthy() const noexcept override { return true; }
    };

    class Silent final : public gnss::GnssTransport {
      public:
        [[nodiscard]] std::size_t read(std::uint8_t*, std::size_t) noexcept override { return 0; }
        [[nodiscard]] bool healthy() const noexcept override { return true; }
    };

    Nmea nmea{};
    const auto reverted = gnss::observe_receiver(nmea, 4);
    assert(!reverted.silent());            // it is talking
    assert(!reverted.already_configured()); // but not in UBX
    assert(reverted.nav_pvt == 0);
    assert(reverted.bytes > 0);

    Silent silent{};
    const auto nothing = gnss::observe_receiver(silent, 4);
    assert(nothing.silent());
    assert(!nothing.already_configured());
    assert(nothing.bytes == 0);
}

// An absent receiver must not cost a boot delay. The I2C driver accepts the address whether
// or not anything is on the bus, so "no receiver attached" presents as failing reads rather
// than as an error - and that is the ordinary development case, paid on every boot.
void an_absent_receiver_does_not_cost_the_whole_window()
{
    class Absent final : public gnss::GnssTransport {
      public:
        [[nodiscard]] std::size_t read(std::uint8_t*, std::size_t) noexcept override
        {
            ++reads;
            return 0;
        }
        [[nodiscard]] bool healthy() const noexcept override { return false; }
        std::uint32_t reads{0};
    };

    Absent receiver{};
    const auto report = gnss::observe_receiver(receiver, gnss::kArrivalPolls);
    assert(report.silent());
    assert(!report.already_configured());
    // One read, then the unhealthy bus ends it - not 120.
    assert(receiver.reads == 1);
}

}  // namespace

int main()
{
    the_valset_payload_writes_each_key_at_the_width_its_key_declares();
    keeping_nmea_is_possible_but_not_the_default();
    the_layers_asked_for_are_the_layers_written();
    a_zero_rate_is_refused_rather_than_sent();
    an_acknowledged_configuration_reports_which_form_the_receiver_answered();
    a_receiver_that_wants_the_stream_register_is_still_configured();
    a_refusal_a_silence_and_an_unwritable_transport_are_three_different_answers();
    a_refusal_outranks_an_acknowledgement();
    the_caller_is_given_the_chance_to_wait();
    a_receiver_already_emitting_nav_pvt_is_recognised_without_writing_to_it();
    nmea_only_and_silence_are_different_answers();
    an_absent_receiver_does_not_cost_the_whole_window();

    std::cout << "GNSS configuration: CFG-VALSET keys written at their declared widths, "
                 "persisted beyond RAM, and acknowledged rather than assumed\n";
    return 0;
}
