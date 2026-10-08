#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::gnss {

inline constexpr std::uint8_t kSyncChar1 = 0xB5;
inline constexpr std::uint8_t kSyncChar2 = 0x62;

// UBX-NAV-PVT is 92 bytes and is the only high-rate message this timer needs. The ceiling is
// deliberately close to it: a frame declaring more than this is refused rather than buffered,
// because a corrupt length field is far likelier than a message we want.
inline constexpr std::size_t kMaximumPayloadBytes = 128;

inline constexpr std::uint8_t kClassNav = 0x01;
inline constexpr std::uint8_t kIdNavPvt = 0x07;
inline constexpr std::uint16_t kNavPvtLength = 92;

struct UbxMessage {
    std::uint8_t message_class{0};
    std::uint8_t message_id{0};
    std::uint16_t length{0};
    const std::uint8_t* payload{nullptr};
};

// Counted rather than hidden, because every one of these is a symptom worth seeing in
// diagnostics: a rising checksum count is a wiring or baud problem, and discarded bytes
// between frames mean the receiver is emitting something we did not ask for.
struct UbxParserCounters {
    std::uint32_t frames{0};
    std::uint32_t checksum_errors{0};
    std::uint32_t oversized_frames{0};
    std::uint32_t discarded_bytes{0};
};

// Incremental UBX framing with bounded storage.
//
// Deliberately byte-at-a-time. It makes the parser indifferent to how a transport chunks its
// reads, which matters because I2C and UART fragment differently, and it means a frame split
// across any number of reads cannot be mishandled.
class UbxParser {
  public:
    // Returns true when this byte completed a frame whose checksum verified. The frame is then
    // readable through message() until the next call.
    [[nodiscard]] bool consume(std::uint8_t byte) noexcept;

    [[nodiscard]] UbxMessage message() const noexcept;
    [[nodiscard]] const UbxParserCounters& counters() const noexcept;
    void reset() noexcept;

  private:
    enum class State : std::uint8_t {
        sync1,
        sync2,
        message_class,
        message_id,
        length_low,
        length_high,
        payload,
        checksum_a,
        checksum_b,
    };

    void begin_hunting() noexcept;
    void accumulate(std::uint8_t byte) noexcept;

    std::array<std::uint8_t, kMaximumPayloadBytes> payload_{};
    State state_{State::sync1};
    std::uint8_t class_{0};
    std::uint8_t id_{0};
    std::uint16_t length_{0};
    std::uint16_t received_{0};
    std::uint8_t checksum_a_{0};
    std::uint8_t checksum_b_{0};
    std::uint8_t expected_checksum_a_{0};
    UbxParserCounters counters_{};
    bool message_ready_{false};
};

// Fills the position, velocity and time fields from UBX-NAV-PVT. Offsets are from the u-blox
// M9 interface description, UBX-21022436 section 3.15.11.
//
// Deliberately does not judge the fix: arrival time, sequence numbering and whether a fix is
// fit for timing belong to the quality rules, not to decoding. False only when the message is
// not a NAV-PVT of the expected length.
[[nodiscard]] bool decode_nav_pvt(const UbxMessage& message, domain::GnssFix& fix) noexcept;

static_assert(std::is_trivially_copyable_v<UbxParserCounters>);

}  // namespace track_timer::gnss
