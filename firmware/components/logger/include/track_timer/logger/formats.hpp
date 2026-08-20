#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/settings/settings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace track_timer::logger {

// 2: session and rest durations widened from uint16 minutes to uint32 seconds, which
// changes both the meaning and the size of every meta record. Logs written before this
// are rejected by their version rather than silently misread as very short sessions.
// 3: peak G on the session summary. Peaks recorded before the gravity reference was
// tracked with the gyroscope latched tilt as acceleration and had left and right swapped,
// so older summaries are refused by version rather than migrated - they are wrong, not old.
inline constexpr std::uint16_t kLogFormatVersion = 3;
inline constexpr std::int64_t kUnavailableUtcNs = -1;
inline constexpr std::size_t kSessionIdentifierCapacity = 32;
// Well beyond a road car on a circuit, and far short of anything a working sensor reports.
inline constexpr float kMaximumCrediblePeakG = 10.0F;
inline constexpr std::size_t kFirmwareCommitCapacity = 41;
inline constexpr std::size_t kProfileNameCapacity = 32;
inline constexpr std::size_t kTrackFingerprintCapacity = 17;

inline constexpr std::string_view kMetaFileName{"meta.json"};
inline constexpr std::string_view kGnssFileName{"gnss.csv"};
inline constexpr std::string_view kEventsFileName{"events.csv"};
inline constexpr std::string_view kSummaryFileName{"summary.json"};
inline constexpr std::string_view kSummaryLapsFileName{"summary_laps.csv"};

inline constexpr std::string_view kGnssCsvHeaderV1{
    "schema_version,record_sequence,measurement_time_ns,arrival_monotonic_us,"
    "latitude_deg,longitude_deg,height_m,speed_mps,heading_deg,horizontal_accuracy_m,"
    "speed_accuracy_mps,heading_accuracy_deg,fix_sequence,valid_flags,num_satellites,"
    "fix_type,accepted_for_timing,reject_reason"};
inline constexpr std::string_view kEventsCsvHeaderV1{
    "schema_version,record_sequence,ordering_monotonic_us,event_type,lap_index,"
    "measurement_time_ns,lap_duration_ns,segment_sequence_0,segment_sequence_1,"
    "intersection_fraction,quality_flags,session_elapsed_ms,session_overrun_ms"};
inline constexpr std::string_view kSummaryLapsCsvHeaderV1{
    "schema_version,lap_index,lap_duration_ns,event_record_sequence,segment_sequence_0,"
    "segment_sequence_1,quality_flags"};

enum class ResetReason : std::uint8_t {
    unknown,
    power_on,
    software,
    watchdog,
    brownout,
    panic,
};

enum class SessionEventType : std::uint8_t {
    session_started,
    lap_crossing,
    overtime_started,
    session_stopped,
    rest_started,
    rest_completed,
    diagnostic,
};

enum class SessionCompletionReason : std::uint8_t {
    none,
    driver_stop,
    reset_recovery,
    pit_entry,
};

enum class SummaryIntegrity : std::uint8_t {
    complete,
    partial_log,
};

enum DegradedSubsystemFlag : std::uint32_t {
    degraded_none = 0,
    degraded_gnss = 1U << 0U,
    degraded_storage = 1U << 1U,
    degraded_imu = 1U << 2U,
    degraded_touch = 1U << 3U,
    degraded_rtc = 1U << 4U,
};

struct SessionSettingsV1 {
    std::uint16_t settings_schema_version{settings::kCurrentSettingsVersion};
    std::uint32_t session_duration_seconds{0};
    std::uint32_t rest_duration_seconds{0};
    std::uint16_t launch_sensitivity_milli_g{0};
    std::uint16_t average_lap_seconds{0};
    std::uint8_t day_brightness_percent{0};
    std::uint8_t night_brightness_percent{0};
    settings::OperatingMode operating_mode{settings::OperatingMode::timer};
    settings::OrientationMode orientation{settings::OrientationMode::fixed_0};
    bool auto_dim_enabled{false};
    settings::LowerDisplayMode lower_display{settings::LowerDisplayMode::elapsed};
    bool trackday_mode_enabled{false};
    settings::LapBoundaryMode lap_boundary{settings::LapBoundaryMode::finish};
    bool pit_exit_auto_start_enabled{false};
    bool pit_entry_auto_stop_enabled{false};
    std::array<char, settings::kTrackIdentifierCapacity> selected_track_id{};
};

struct SessionMetaV1 {
    std::uint16_t schema_version{kLogFormatVersion};
    std::uint16_t record_size_bytes{0};
    std::array<char, kSessionIdentifierCapacity> session_id{};
    std::array<char, kFirmwareCommitCapacity> firmware_commit{};
    std::array<char, kProfileNameCapacity> hardware_profile{};
    std::array<char, kProfileNameCapacity> gnss_profile{};
    std::uint16_t gnss_update_rate_hz{0};
    ResetReason reset_reason{ResetReason::unknown};
    std::uint16_t track_schema_version{0};
    std::array<char, settings::kTrackIdentifierCapacity> track_id{};
    std::array<char, kTrackFingerprintCapacity> track_fingerprint{};
    std::int64_t start_utc_ns{kUnavailableUtcNs};
    std::int64_t end_utc_ns{kUnavailableUtcNs};
    SessionSettingsV1 settings{};
};

struct GnssRecordV1 {
    std::uint16_t schema_version{kLogFormatVersion};
    std::uint16_t record_size_bytes{0};
    std::uint32_t record_sequence{0};
    std::int64_t measurement_time_ns{domain::kUnavailableTime};
    std::int64_t arrival_monotonic_us{domain::kUnavailableTime};
    double latitude_deg{0.0};
    double longitude_deg{0.0};
    float height_m{0.0F};
    float speed_mps{0.0F};
    float heading_deg{0.0F};
    float horizontal_accuracy_m{0.0F};
    float speed_accuracy_mps{0.0F};
    float heading_accuracy_deg{0.0F};
    std::uint32_t fix_sequence{0};
    std::uint32_t valid_flags{0};
    std::uint16_t num_satellites{0};
    domain::FixType fix_type{domain::FixType::no_fix};
    domain::FixRejectReason reject_reason{domain::FixRejectReason::none};
    bool accepted_for_timing{false};
};

struct EventRecordV1 {
    std::uint16_t schema_version{kLogFormatVersion};
    std::uint16_t record_size_bytes{0};
    std::uint32_t record_sequence{0};
    std::int64_t ordering_monotonic_us{domain::kUnavailableTime};
    SessionEventType event_type{SessionEventType::diagnostic};
    std::uint32_t lap_index{0};
    std::int64_t measurement_time_ns{domain::kUnavailableTime};
    std::int64_t lap_duration_ns{domain::kUnavailableTime};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    double intersection_fraction{0.0};
    std::uint32_t quality_flags{0};
    std::int64_t session_elapsed_ms{domain::kUnavailableTime};
    std::int64_t session_overrun_ms{domain::kUnavailableTime};
};

// What the car pulled during a session. Vertical is kept apart from the horizontal pair,
// and up from down, because a kerb strike and a compression are different events.
struct SummaryPeakG {
    float acceleration_g{0.0F};
    float braking_g{0.0F};
    float left_g{0.0F};
    float right_g{0.0F};
    float up_g{0.0F};
    float down_g{0.0F};
    float total_g{0.0F};
};

struct SessionSummaryV1 {
    std::uint16_t schema_version{kLogFormatVersion};
    std::uint16_t record_size_bytes{0};
    std::array<char, kSessionIdentifierCapacity> session_id{};
    std::int64_t session_duration_ms{0};
    std::int64_t session_overrun_ms{0};
    SessionCompletionReason completion_reason{SessionCompletionReason::none};
    SummaryIntegrity integrity{SummaryIntegrity::complete};
    std::uint32_t degraded_subsystems{degraded_none};
    std::uint32_t lap_count{0};
    std::uint32_t best_lap_index{0};
    std::int64_t best_lap_duration_ns{domain::kUnavailableTime};
    std::uint32_t source_first_record_sequence{0};
    std::uint32_t source_last_record_sequence{0};
    std::uint32_t source_gnss_record_count{0};
    std::uint32_t source_accepted_fix_count{0};
    std::uint32_t source_rejected_fix_count{0};
    std::uint32_t source_event_record_count{0};
    std::uint32_t logger_dropped_record_count{0};
    std::uint32_t logger_write_failure_count{0};
    SummaryPeakG peaks{};
};

struct SummaryLapRecordV1 {
    std::uint16_t schema_version{kLogFormatVersion};
    std::uint16_t record_size_bytes{0};
    std::uint32_t lap_index{0};
    std::int64_t lap_duration_ns{domain::kUnavailableTime};
    std::uint32_t event_record_sequence{0};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    std::uint32_t quality_flags{0};
};

[[nodiscard]] SessionSettingsV1 make_session_settings(
    const settings::DeviceSettings& source) noexcept;
[[nodiscard]] GnssRecordV1 make_gnss_record(const domain::GnssFix& source,
                                            std::uint32_t record_sequence) noexcept;
[[nodiscard]] EventRecordV1 make_lap_event_record(const domain::LapEvent& source,
                                                  std::uint32_t record_sequence,
                                                  std::int64_t ordering_monotonic_us,
                                                  std::int64_t session_elapsed_ms,
                                                  std::int64_t session_overrun_ms) noexcept;
[[nodiscard]] SummaryLapRecordV1 make_summary_lap_record(
    const EventRecordV1& source) noexcept;

[[nodiscard]] bool valid_meta(const SessionMetaV1& record) noexcept;
[[nodiscard]] bool valid_gnss_record(const GnssRecordV1& record) noexcept;
[[nodiscard]] bool valid_event_record(const EventRecordV1& record) noexcept;
[[nodiscard]] bool valid_summary(const SessionSummaryV1& record) noexcept;
[[nodiscard]] bool valid_summary_lap(const SummaryLapRecordV1& record) noexcept;

[[nodiscard]] const char* fix_type_name(domain::FixType value) noexcept;
[[nodiscard]] const char* reject_reason_name(domain::FixRejectReason value) noexcept;
[[nodiscard]] const char* event_type_name(SessionEventType value) noexcept;

static_assert(std::is_trivially_copyable_v<SessionMetaV1>);
static_assert(std::is_trivially_copyable_v<GnssRecordV1>);
static_assert(std::is_trivially_copyable_v<EventRecordV1>);
static_assert(std::is_trivially_copyable_v<SessionSummaryV1>);
static_assert(std::is_trivially_copyable_v<SummaryLapRecordV1>);
static_assert(sizeof(GnssRecordV1) <= 128);
static_assert(sizeof(EventRecordV1) <= 96);
static_assert(sizeof(SummaryLapRecordV1) <= 48);

}  // namespace track_timer::logger
