#include "track_timer/gnss/ubx.hpp"

#include <cstring>

namespace track_timer::gnss {
namespace {

[[nodiscard]] std::uint32_t read_u32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0]) |
           static_cast<std::uint32_t>(bytes[1]) << 8U |
           static_cast<std::uint32_t>(bytes[2]) << 16U |
           static_cast<std::uint32_t>(bytes[3]) << 24U;
}

[[nodiscard]] std::int32_t read_i32(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::int32_t>(read_u32(bytes));
}

// Offsets within the NAV-PVT payload, from UBX-21022436 section 3.15.11.
constexpr std::size_t kOffsetITow = 0;
constexpr std::size_t kOffsetValid = 11;
constexpr std::size_t kOffsetNano = 16;
constexpr std::size_t kOffsetFixType = 20;
constexpr std::size_t kOffsetFlags = 21;
constexpr std::size_t kOffsetNumSv = 23;
constexpr std::size_t kOffsetLongitude = 24;
constexpr std::size_t kOffsetLatitude = 28;
constexpr std::size_t kOffsetHeight = 32;
constexpr std::size_t kOffsetHorizontalAccuracy = 40;
constexpr std::size_t kOffsetGroundSpeed = 60;
constexpr std::size_t kOffsetHeading = 64;
constexpr std::size_t kOffsetSpeedAccuracy = 68;
constexpr std::size_t kOffsetHeadingAccuracy = 72;

}  // namespace

bool UbxParser::consume(const std::uint8_t byte) noexcept
{
    message_ready_ = false;

    switch (state_) {
    case State::sync1:
        if (byte == kSyncChar1) {
            state_ = State::sync2;
        }
        else if (counters_.discarded_bytes < UINT32_MAX) {
            // Anything outside a frame is counted: a receiver emitting NMEA we did not ask
            // for looks exactly like this.
            ++counters_.discarded_bytes;
        }
        return false;

    case State::sync2:
        if (byte == kSyncChar2) {
            state_ = State::message_class;
            checksum_a_ = 0;
            checksum_b_ = 0;
            return false;
        }
        // A second 0xB5 is a fresh candidate rather than a failure, so "B5 B5 62" still
        // frames. Treating it as garbage would drop every frame preceded by a stray sync byte.
        if (byte == kSyncChar1) {
            ++counters_.discarded_bytes;
            return false;
        }
        ++counters_.discarded_bytes;
        begin_hunting();
        return false;

    case State::message_class:
        class_ = byte;
        accumulate(byte);
        state_ = State::message_id;
        return false;

    case State::message_id:
        id_ = byte;
        accumulate(byte);
        state_ = State::length_low;
        return false;

    case State::length_low:
        length_ = byte;
        accumulate(byte);
        state_ = State::length_high;
        return false;

    case State::length_high:
        length_ = static_cast<std::uint16_t>(length_ |
                                             static_cast<std::uint16_t>(byte) << 8U);
        accumulate(byte);
        if (length_ > kMaximumPayloadBytes) {
            // Refused rather than buffered, and we return to hunting rather than skipping the
            // declared length: a length field this large is far more likely to be corruption
            // than a real message, and trusting it would discard good bytes behind it.
            ++counters_.oversized_frames;
            begin_hunting();
            return false;
        }
        received_ = 0;
        state_ = length_ == 0 ? State::checksum_a : State::payload;
        return false;

    case State::payload:
        payload_[received_++] = byte;
        accumulate(byte);
        if (received_ >= length_) {
            state_ = State::checksum_a;
        }
        return false;

    case State::checksum_a:
        expected_checksum_a_ = byte;
        state_ = State::checksum_b;
        return false;

    case State::checksum_b:
        if (expected_checksum_a_ != checksum_a_ || byte != checksum_b_) {
            ++counters_.checksum_errors;
            begin_hunting();
            return false;
        }
        ++counters_.frames;
        begin_hunting();
        message_ready_ = true;
        return true;
    }

    begin_hunting();
    return false;
}

void UbxParser::begin_hunting() noexcept
{
    // Always back to hunting the sync pair, never stalled mid-frame. This is what makes
    // malformed data unable to prevent recovery of the frames behind it.
    state_ = State::sync1;
    received_ = 0;
}

void UbxParser::accumulate(const std::uint8_t byte) noexcept
{
    // 8-bit Fletcher over class, id, length and payload, per the UBX framing.
    checksum_a_ = static_cast<std::uint8_t>(checksum_a_ + byte);
    checksum_b_ = static_cast<std::uint8_t>(checksum_b_ + checksum_a_);
}

UbxMessage UbxParser::message() const noexcept
{
    if (!message_ready_) {
        return {};
    }
    return {class_, id_, length_, payload_.data()};
}

const UbxParserCounters& UbxParser::counters() const noexcept { return counters_; }

void UbxParser::reset() noexcept
{
    begin_hunting();
    counters_ = {};
    message_ready_ = false;
}

bool decode_nav_pvt(const UbxMessage& message, domain::GnssFix& fix) noexcept
{
    if (message.message_class != kClassNav || message.message_id != kIdNavPvt ||
        message.length != kNavPvtLength || message.payload == nullptr) {
        return false;
    }
    const auto* payload = message.payload;

    // Time of week plus the signed sub-millisecond correction. Relative durations are what lap
    // timing needs, and both crossings use this same clock, so a week rollover cannot change a
    // lap time computed across it.
    const auto itow_ms = static_cast<std::int64_t>(read_u32(payload + kOffsetITow));
    const auto nano = static_cast<std::int64_t>(read_i32(payload + kOffsetNano));
    fix.measurement_time_ns = itow_ms * 1'000'000 + nano;

    fix.latitude_deg = static_cast<double>(read_i32(payload + kOffsetLatitude)) * 1e-7;
    fix.longitude_deg = static_cast<double>(read_i32(payload + kOffsetLongitude)) * 1e-7;
    fix.height_m = static_cast<float>(read_i32(payload + kOffsetHeight)) / 1000.0F;
    fix.speed_mps = static_cast<float>(read_i32(payload + kOffsetGroundSpeed)) / 1000.0F;
    fix.heading_deg = static_cast<float>(read_i32(payload + kOffsetHeading)) * 1e-5F;
    fix.horizontal_accuracy_m =
        static_cast<float>(read_u32(payload + kOffsetHorizontalAccuracy)) / 1000.0F;
    fix.speed_accuracy_mps =
        static_cast<float>(read_u32(payload + kOffsetSpeedAccuracy)) / 1000.0F;
    fix.heading_accuracy_deg =
        static_cast<float>(read_u32(payload + kOffsetHeadingAccuracy)) * 1e-5F;
    fix.num_satellites = payload[kOffsetNumSv];

    const auto raw_fix_type = payload[kOffsetFixType];
    fix.fix_type = raw_fix_type <= static_cast<std::uint8_t>(domain::FixType::time_only)
                       ? static_cast<domain::FixType>(raw_fix_type)
                       : domain::FixType::no_fix;

    // Both flag bytes are kept intact rather than interpreted here. gnssFixOK and the UTC
    // validity bits are the raw material the quality rules work from, and squashing them to a
    // boolean now would throw away the reason a fix was refused later.
    fix.valid_flags = static_cast<std::uint32_t>(payload[kOffsetValid]) |
                      static_cast<std::uint32_t>(payload[kOffsetFlags]) << 8U;
    return true;
}

}  // namespace track_timer::gnss
