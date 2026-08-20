#pragma once

#include "track_timer/logger/formats.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::logger {

// Session summaries are appended to one file as framed records, so a session that ends
// badly can only ever damage the record it was writing.
//
// Each frame carries its own magic, version, length and checksum. A reader that meets a
// torn or corrupt frame stops trusting that frame and keeps everything before it, which is
// the behaviour that matters when a card is pulled mid-write: the sessions already on the
// card survive.
//
// The payload is the record's bytes. That makes this file device-internal rather than a
// portable export - a host tool cannot assume the same layout - which is deliberate:
// human-readable session output belongs with the per-session log files in #38, and paying
// for a field-by-field codec here would buy portability nothing yet needs.
inline constexpr std::array<std::uint8_t, 4> kSummaryFrameMagic{'T', 'S', 'S', 'F'};
inline constexpr std::size_t kSummaryFrameHeaderSize = 12;
inline constexpr std::size_t kSummaryFrameSize =
    kSummaryFrameHeaderSize + sizeof(SessionSummaryV1);

static_assert(std::is_trivially_copyable_v<SessionSummaryV1>,
              "the frame payload is the record's bytes");

struct SummaryFrame {
    std::array<std::uint8_t, kSummaryFrameSize> bytes{};
    std::size_t size{0};
};

enum class FrameResult : std::uint8_t {
    ready,
    too_short,       // fewer bytes than a frame needs; a torn tail looks like this
    bad_magic,
    unsupported_version,
    bad_checksum,
    invalid_record,  // framed correctly but the record fails its own validator
};

// Refuses to frame a record that would not survive its own validator, so nothing invalid
// reaches the card in the first place.
[[nodiscard]] bool encode_summary_frame(const SessionSummaryV1& summary,
                                        SummaryFrame& frame) noexcept;

[[nodiscard]] FrameResult decode_summary_frame(const std::uint8_t* bytes, std::size_t size,
                                               SessionSummaryV1& summary) noexcept;

struct SummaryScanReport {
    std::size_t accepted{0};
    // Frames that were intact enough to skip past but not to trust. Counted rather than
    // hidden, because a card quietly dropping records should be visible in diagnostics.
    std::size_t skipped{0};
    // A final frame shorter than a whole one, which is what a pull mid-write leaves behind.
    bool truncated_tail{false};
};

// Walks a buffer of appended frames. Frames are fixed size, so a corrupt one in the middle
// can be stepped over and the records after it still recovered - stopping at the first bad
// frame would throw away good sessions to protect a bad one.
template <typename Sink>
SummaryScanReport scan_summary_frames(const std::uint8_t* bytes, std::size_t size,
                                      Sink&& sink) noexcept
{
    SummaryScanReport report{};
    std::size_t offset = 0;
    while (offset < size) {
        const auto remaining = size - offset;
        SessionSummaryV1 summary{};
        const auto result = decode_summary_frame(bytes + offset, remaining, summary);
        if (result == FrameResult::too_short) {
            report.truncated_tail = true;
            break;
        }
        if (result == FrameResult::ready) {
            sink(summary);
            ++report.accepted;
        }
        else {
            ++report.skipped;
        }
        offset += kSummaryFrameSize;
    }
    return report;
}

}  // namespace track_timer::logger
