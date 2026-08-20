#include "track_timer/logger/summary_frame.hpp"

#include <algorithm>
#include <cstring>

namespace track_timer::logger {
namespace {

// FNV-1a, the same shape the settings blob uses. Enough to catch a torn write or a flipped
// bit, which is what this guards against; it is not a security check.
[[nodiscard]] std::uint32_t checksum(const std::uint8_t* bytes, const std::size_t size) noexcept
{
    std::uint32_t hash = 2'166'136'261U;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16'777'619U;
    }
    return hash;
}

void put_u16(std::uint8_t* output, const std::uint16_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void put_u32(std::uint8_t* output, const std::uint32_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    output[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    output[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] std::uint16_t get_u16(const std::uint8_t* input) noexcept
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(input[0]) |
                                      static_cast<std::uint16_t>(input[1]) << 8U);
}

[[nodiscard]] std::uint32_t get_u32(const std::uint8_t* input) noexcept
{
    return static_cast<std::uint32_t>(input[0]) |
           static_cast<std::uint32_t>(input[1]) << 8U |
           static_cast<std::uint32_t>(input[2]) << 16U |
           static_cast<std::uint32_t>(input[3]) << 24U;
}

}  // namespace

bool encode_summary_frame(const SessionSummaryV1& summary, SummaryFrame& frame) noexcept
{
    frame = {};
    if (!valid_summary(summary)) {
        return false;
    }

    std::copy(kSummaryFrameMagic.begin(), kSummaryFrameMagic.end(), frame.bytes.begin());
    put_u16(frame.bytes.data() + 4, kLogFormatVersion);
    put_u16(frame.bytes.data() + 6, static_cast<std::uint16_t>(sizeof(SessionSummaryV1)));
    std::memcpy(frame.bytes.data() + kSummaryFrameHeaderSize, &summary,
                sizeof(SessionSummaryV1));
    put_u32(frame.bytes.data() + 8,
            checksum(frame.bytes.data() + kSummaryFrameHeaderSize, sizeof(SessionSummaryV1)));
    frame.size = kSummaryFrameSize;
    return true;
}

FrameResult decode_summary_frame(const std::uint8_t* const bytes, const std::size_t size,
                                 SessionSummaryV1& summary) noexcept
{
    // A torn tail is short rather than corrupt, and the caller treats the two the same way:
    // keep what came before, stop reading here.
    if (bytes == nullptr || size < kSummaryFrameSize) {
        return FrameResult::too_short;
    }
    if (!std::equal(kSummaryFrameMagic.begin(), kSummaryFrameMagic.end(), bytes)) {
        return FrameResult::bad_magic;
    }
    if (get_u16(bytes + 4) != kLogFormatVersion) {
        return FrameResult::unsupported_version;
    }
    if (get_u16(bytes + 6) != sizeof(SessionSummaryV1)) {
        // The record grew or shrank under a version that did not move, which means the
        // file was written by a build this one cannot read.
        return FrameResult::unsupported_version;
    }
    if (get_u32(bytes + 8) !=
        checksum(bytes + kSummaryFrameHeaderSize, sizeof(SessionSummaryV1))) {
        return FrameResult::bad_checksum;
    }

    SessionSummaryV1 candidate{};
    std::memcpy(&candidate, bytes + kSummaryFrameHeaderSize, sizeof(SessionSummaryV1));
    if (!valid_summary(candidate)) {
        // Intact on the wire and nonsense as a record: a checksum cannot tell you a session
        // lasted a negative length of time.
        return FrameResult::invalid_record;
    }
    summary = candidate;
    return FrameResult::ready;
}

}  // namespace track_timer::logger
