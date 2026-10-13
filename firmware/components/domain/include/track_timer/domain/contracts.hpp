#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::domain {

inline constexpr std::int64_t kUnavailableTime = -1;

enum class FixType : std::uint8_t {
    no_fix,
    dead_reckoning,
    fix_2d,
    fix_3d,
    gnss_dead_reckoning,
    time_only,
};

enum class FixRejectReason : std::uint8_t {
    none,
    invalid_status,
    stale,
    non_monotonic_sequence,
    non_monotonic_time,
    excessive_accuracy,
    implausible_motion,
};

// The receiver's opinion of the wall clock, kept whole.
//
// Not eight loose fields on GnssFix, because they are only ever meaningful together: a
// clock reading made of an hour from one epoch and a minute from another is not a
// partially-correct time, it is a wrong one. `valid` therefore gates the whole struct.
//
// This is the receiver's answer, not a derived one. GnssFix::measurement_time_ns is GPS
// time of week: no date, a rollover every Sunday, and ahead of UTC by the accumulated
// leap-second count. The receiver holds the almanac and applies that offset, so the
// honest thing is to read what it decided rather than reconstruct it here.
struct UtcTime {
    // The fraction of `second` we are actually at, in nanoseconds. Signed, and negative
    // means the true instant is just *before* `second` - so `second` on its own can be up
    // to a second early.
    //
    // This is NAV-PVT's `nano`, the field #193 was about. It was previously folded into
    // iTOW as though it corrected it, which drove measurement time 760 ms backwards. It
    // belongs here and nowhere else: a wall clock is the only consumer that wants it.
    std::int32_t nanosecond{0};
    std::uint16_t year{0};
    std::uint8_t month{0};
    std::uint8_t day{0};
    std::uint8_t hour{0};    // 0-23; the receiver reports 24-hour already
    std::uint8_t minute{0};
    // 0-60. Sixty is not a bug: a positive leap second is inserted as 23:59:60 UTC, and a
    // display that assumes 0-59 renders that as something else.
    std::uint8_t second{0};
    // The receiver says both validTime and fullyResolved. A clock may trust this long
    // before a position fix exists, which is why it is separate from FixType.
    bool valid{false};
};

struct GnssFix {
    std::int64_t measurement_time_ns{kUnavailableTime};
    std::int64_t arrival_monotonic_us{kUnavailableTime};
    double latitude_deg{0.0};
    double longitude_deg{0.0};
    float height_m{0.0F};
    float speed_mps{0.0F};
    float heading_deg{0.0F};
    float horizontal_accuracy_m{0.0F};
    float speed_accuracy_mps{0.0F};
    float heading_accuracy_deg{0.0F};
    std::uint32_t sequence_number{0};
    std::uint32_t valid_flags{0};
    std::uint16_t num_satellites{0};
    FixType fix_type{FixType::no_fix};
    FixRejectReason reject_reason{FixRejectReason::none};
    bool accepted_for_timing{false};
    // Appended, like every field before it: this struct is aggregate-initialised at call
    // sites and a field inserted in the middle silently reassigns positional initialisers.
    UtcTime utc{};
};

struct LapEvent {
    std::uint32_t lap_index{0};
    std::int64_t crossing_measurement_time_ns{kUnavailableTime};
    std::int64_t lap_duration_ns{kUnavailableTime};
    double intersection_fraction{0.0};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    std::uint32_t quality_flags{0};
};

enum class GnssHealth : std::uint8_t {
    unavailable,
    searching,
    poor,
    good,
    stale,
};

struct UiSnapshot {
    std::int64_t session_remaining_ms{kUnavailableTime};
    std::int64_t current_lap_ms{kUnavailableTime};
    std::int64_t previous_lap_ms{kUnavailableTime};
    std::int64_t best_lap_ms{kUnavailableTime};
    std::uint32_t lap_index{0};
    GnssHealth gnss_health{GnssHealth::unavailable};
    bool session_active{false};
    bool logging_available{false};
};

enum class LogRecordType : std::uint8_t {
    gnss_fix,
    lap_event,
    session_event,
    diagnostic,
};

inline constexpr std::size_t kMaxLogPayloadBytes = 96;

struct LogRecord {
    std::int64_t ordering_monotonic_us{kUnavailableTime};
    std::uint32_t sequence_number{0};
    std::uint16_t payload_size{0};
    LogRecordType type{LogRecordType::diagnostic};
    std::array<std::uint8_t, kMaxLogPayloadBytes> payload{};
};

namespace queue_capacity {

// 64 fixes provide 2.56 seconds of headroom at 25 Hz.
inline constexpr std::size_t gnss_fixes = 64;
inline constexpr std::size_t lap_events = 16;
inline constexpr std::size_t gate_crossing_records = 64;
inline constexpr std::size_t log_records = 256;
inline constexpr std::size_t ui_snapshots = 2;

}  // namespace queue_capacity

static_assert(queue_capacity::gnss_fixes >= 50);
static_assert(queue_capacity::gate_crossing_records >= 16);
static_assert(std::is_trivially_copyable_v<UtcTime>);
static_assert(std::is_trivially_copyable_v<GnssFix>);
static_assert(std::is_trivially_copyable_v<LapEvent>);
static_assert(std::is_trivially_copyable_v<UiSnapshot>);
static_assert(std::is_trivially_copyable_v<LogRecord>);
static_assert(sizeof(GnssFix) <= 128);
static_assert(sizeof(LogRecord) <= 128);

}  // namespace track_timer::domain
