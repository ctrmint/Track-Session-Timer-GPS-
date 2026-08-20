#include "track_timer/logger/formats.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace track_timer::logger {
namespace {

template <std::size_t Capacity>
bool terminated(const std::array<char, Capacity>& value) noexcept
{
    return std::find(value.begin(), value.end(), '\0') != value.end();
}

template <std::size_t Capacity>
bool non_empty(const std::array<char, Capacity>& value) noexcept
{
    return terminated(value) && value[0] != '\0';
}

template <std::size_t Capacity>
std::size_t text_length(const std::array<char, Capacity>& value) noexcept
{
    const auto terminator = std::find(value.begin(), value.end(), '\0');
    return static_cast<std::size_t>(terminator - value.begin());
}

bool valid_fix_type(const domain::FixType value) noexcept
{
    switch (value) {
    case domain::FixType::no_fix:
    case domain::FixType::dead_reckoning:
    case domain::FixType::fix_2d:
    case domain::FixType::fix_3d:
    case domain::FixType::gnss_dead_reckoning:
    case domain::FixType::time_only:
        return true;
    }
    return false;
}

bool valid_reject_reason(const domain::FixRejectReason value) noexcept
{
    switch (value) {
    case domain::FixRejectReason::none:
    case domain::FixRejectReason::invalid_status:
    case domain::FixRejectReason::stale:
    case domain::FixRejectReason::non_monotonic_sequence:
    case domain::FixRejectReason::non_monotonic_time:
    case domain::FixRejectReason::excessive_accuracy:
    case domain::FixRejectReason::implausible_motion:
        return true;
    }
    return false;
}

bool valid_event_type(const SessionEventType value) noexcept
{
    switch (value) {
    case SessionEventType::session_started:
    case SessionEventType::lap_crossing:
    case SessionEventType::overtime_started:
    case SessionEventType::session_stopped:
    case SessionEventType::rest_started:
    case SessionEventType::rest_completed:
    case SessionEventType::diagnostic:
        return true;
    }
    return false;
}

bool valid_reset_reason(const ResetReason value) noexcept
{
    switch (value) {
    case ResetReason::unknown:
    case ResetReason::power_on:
    case ResetReason::software:
    case ResetReason::watchdog:
    case ResetReason::brownout:
    case ResetReason::panic:
        return true;
    }
    return false;
}

bool valid_completion_reason(const SessionCompletionReason value) noexcept
{
    switch (value) {
    case SessionCompletionReason::none:
    case SessionCompletionReason::driver_stop:
    case SessionCompletionReason::reset_recovery:
    case SessionCompletionReason::pit_entry:
        return true;
    }
    return false;
}

bool valid_integrity(const SummaryIntegrity value) noexcept
{
    switch (value) {
    case SummaryIntegrity::complete:
    case SummaryIntegrity::partial_log:
        return true;
    }
    return false;
}

bool valid_settings_record(const SessionSettingsV1& record) noexcept
{
    if (record.settings_schema_version != settings::kCurrentSettingsVersion ||
        !terminated(record.selected_track_id)) {
        return false;
    }

    settings::DeviceSettings source{};
    source.session_duration_seconds = record.session_duration_seconds;
    source.rest_duration_seconds = record.rest_duration_seconds;
    source.launch_sensitivity_milli_g = record.launch_sensitivity_milli_g;
    source.average_lap_seconds = record.average_lap_seconds;
    source.day_brightness_percent = record.day_brightness_percent;
    source.night_brightness_percent = record.night_brightness_percent;
    source.operating_mode = record.operating_mode;
    source.orientation = record.orientation;
    source.auto_dim_enabled = record.auto_dim_enabled;
    source.lower_display = record.lower_display;
    source.trackday_mode_enabled = record.trackday_mode_enabled;
    source.lap_boundary = record.lap_boundary;
    source.pit_exit_auto_start_enabled = record.pit_exit_auto_start_enabled;
    source.pit_entry_auto_stop_enabled = record.pit_entry_auto_stop_enabled;
    source.selected_track_id = record.selected_track_id;
    return settings::valid_settings(source);
}

}  // namespace

SessionSettingsV1 make_session_settings(const settings::DeviceSettings& source) noexcept
{
    SessionSettingsV1 result{};
    result.session_duration_seconds = source.session_duration_seconds;
    result.rest_duration_seconds = source.rest_duration_seconds;
    result.launch_sensitivity_milli_g = source.launch_sensitivity_milli_g;
    result.average_lap_seconds = source.average_lap_seconds;
    result.day_brightness_percent = source.day_brightness_percent;
    result.night_brightness_percent = source.night_brightness_percent;
    result.operating_mode = source.operating_mode;
    result.orientation = source.orientation;
    result.auto_dim_enabled = source.auto_dim_enabled;
    result.lower_display = source.lower_display;
    result.trackday_mode_enabled = source.trackday_mode_enabled;
    result.lap_boundary = source.lap_boundary;
    result.pit_exit_auto_start_enabled = source.pit_exit_auto_start_enabled;
    result.pit_entry_auto_stop_enabled = source.pit_entry_auto_stop_enabled;
    result.selected_track_id = source.selected_track_id;
    return result;
}

GnssRecordV1 make_gnss_record(const domain::GnssFix& source,
                              const std::uint32_t record_sequence) noexcept
{
    GnssRecordV1 result{};
    result.record_size_bytes = sizeof(GnssRecordV1);
    result.record_sequence = record_sequence;
    result.measurement_time_ns = source.measurement_time_ns;
    result.arrival_monotonic_us = source.arrival_monotonic_us;
    result.latitude_deg = source.latitude_deg;
    result.longitude_deg = source.longitude_deg;
    result.height_m = source.height_m;
    result.speed_mps = source.speed_mps;
    result.heading_deg = source.heading_deg;
    result.horizontal_accuracy_m = source.horizontal_accuracy_m;
    result.speed_accuracy_mps = source.speed_accuracy_mps;
    result.heading_accuracy_deg = source.heading_accuracy_deg;
    result.fix_sequence = source.sequence_number;
    result.valid_flags = source.valid_flags;
    result.num_satellites = source.num_satellites;
    result.fix_type = source.fix_type;
    result.reject_reason = source.reject_reason;
    result.accepted_for_timing = source.accepted_for_timing;
    return result;
}

EventRecordV1 make_lap_event_record(const domain::LapEvent& source,
                                    const std::uint32_t record_sequence,
                                    const std::int64_t ordering_monotonic_us,
                                    const std::int64_t session_elapsed_ms,
                                    const std::int64_t session_overrun_ms) noexcept
{
    EventRecordV1 result{};
    result.record_size_bytes = sizeof(EventRecordV1);
    result.record_sequence = record_sequence;
    result.ordering_monotonic_us = ordering_monotonic_us;
    result.event_type = SessionEventType::lap_crossing;
    result.lap_index = source.lap_index;
    result.measurement_time_ns = source.crossing_measurement_time_ns;
    result.lap_duration_ns = source.lap_duration_ns;
    result.segment_sequence_0 = source.segment_sequence_0;
    result.segment_sequence_1 = source.segment_sequence_1;
    result.intersection_fraction = source.intersection_fraction;
    result.quality_flags = source.quality_flags;
    result.session_elapsed_ms = session_elapsed_ms;
    result.session_overrun_ms = session_overrun_ms;
    return result;
}

SummaryLapRecordV1 make_summary_lap_record(const EventRecordV1& source) noexcept
{
    SummaryLapRecordV1 result{};
    result.record_size_bytes = sizeof(SummaryLapRecordV1);
    result.lap_index = source.lap_index;
    result.lap_duration_ns = source.lap_duration_ns;
    result.event_record_sequence = source.record_sequence;
    result.segment_sequence_0 = source.segment_sequence_0;
    result.segment_sequence_1 = source.segment_sequence_1;
    result.quality_flags = source.quality_flags;
    return result;
}

bool valid_meta(const SessionMetaV1& record) noexcept
{
    if (record.schema_version != kLogFormatVersion ||
        record.record_size_bytes != sizeof(SessionMetaV1) || !non_empty(record.session_id) ||
        !non_empty(record.firmware_commit) || !non_empty(record.hardware_profile) ||
        !non_empty(record.gnss_profile) || record.gnss_update_rate_hz == 0 ||
        record.gnss_update_rate_hz > 100 || !valid_reset_reason(record.reset_reason) ||
        !valid_settings_record(record.settings)) {
        return false;
    }

    if ((record.start_utc_ns != kUnavailableUtcNs && record.start_utc_ns < 0) ||
        (record.end_utc_ns != kUnavailableUtcNs && record.end_utc_ns < 0) ||
        (record.start_utc_ns != kUnavailableUtcNs && record.end_utc_ns != kUnavailableUtcNs &&
         record.end_utc_ns < record.start_utc_ns)) {
        return false;
    }

    if (!terminated(record.track_id) || !terminated(record.track_fingerprint)) {
        return false;
    }
    const bool has_track = record.track_id[0] != '\0';
    if (!has_track) {
        return record.track_schema_version == 0 && record.track_fingerprint[0] == '\0' &&
               record.settings.selected_track_id[0] == '\0';
    }
    return record.track_schema_version > 0 &&
           text_length(record.track_fingerprint) == kTrackFingerprintCapacity - 1 &&
           record.track_id == record.settings.selected_track_id;
}

bool valid_gnss_record(const GnssRecordV1& record) noexcept
{
    if (record.schema_version != kLogFormatVersion ||
        record.record_size_bytes != sizeof(GnssRecordV1) || record.measurement_time_ns < 0 ||
        record.arrival_monotonic_us < 0 || !valid_fix_type(record.fix_type) ||
        !valid_reject_reason(record.reject_reason)) {
        return false;
    }
    if (!std::isfinite(record.latitude_deg) || record.latitude_deg < -90.0 ||
        record.latitude_deg > 90.0 || !std::isfinite(record.longitude_deg) ||
        record.longitude_deg < -180.0 || record.longitude_deg > 180.0 ||
        !std::isfinite(record.height_m) || !std::isfinite(record.speed_mps) ||
        record.speed_mps < 0.0F || !std::isfinite(record.heading_deg) ||
        record.heading_deg < 0.0F || record.heading_deg >= 360.0F ||
        !std::isfinite(record.horizontal_accuracy_m) ||
        record.horizontal_accuracy_m < 0.0F || !std::isfinite(record.speed_accuracy_mps) ||
        record.speed_accuracy_mps < 0.0F || !std::isfinite(record.heading_accuracy_deg) ||
        record.heading_accuracy_deg < 0.0F) {
        return false;
    }
    return record.accepted_for_timing
               ? record.reject_reason == domain::FixRejectReason::none
               : record.reject_reason != domain::FixRejectReason::none;
}

bool valid_event_record(const EventRecordV1& record) noexcept
{
    if (record.schema_version != kLogFormatVersion ||
        record.record_size_bytes != sizeof(EventRecordV1) || record.ordering_monotonic_us < 0 ||
        !valid_event_type(record.event_type)) {
        return false;
    }
    if (record.event_type != SessionEventType::lap_crossing) {
        return true;
    }
    return record.lap_index > 0 && record.measurement_time_ns >= 0 &&
           record.lap_duration_ns > 0 && record.segment_sequence_1 > record.segment_sequence_0 &&
           std::isfinite(record.intersection_fraction) && record.intersection_fraction >= 0.0 &&
           record.intersection_fraction <= 1.0 && record.session_elapsed_ms >= 0 &&
           record.session_overrun_ms >= 0 &&
           record.session_overrun_ms <= record.session_elapsed_ms;
}

// A peak is a magnitude, so it cannot be negative, and anything beyond what a car can
// physically pull says the reading is wrong rather than remarkable.
bool valid_peak(const float value) noexcept
{
    return value >= 0.0F && value <= kMaximumCrediblePeakG;
}

bool valid_summary(const SessionSummaryV1& record) noexcept
{
    if (!valid_peak(record.peaks.acceleration_g) || !valid_peak(record.peaks.braking_g) ||
        !valid_peak(record.peaks.left_g) || !valid_peak(record.peaks.right_g) ||
        !valid_peak(record.peaks.up_g) || !valid_peak(record.peaks.down_g) ||
        !valid_peak(record.peaks.total_g)) {
        return false;
    }
    if (record.schema_version != kLogFormatVersion ||
        record.record_size_bytes != sizeof(SessionSummaryV1) || !non_empty(record.session_id) ||
        record.session_duration_ms < 0 || record.session_overrun_ms < 0 ||
        record.session_overrun_ms > record.session_duration_ms ||
        !valid_completion_reason(record.completion_reason) ||
        record.completion_reason == SessionCompletionReason::none ||
        !valid_integrity(record.integrity) ||
        (record.degraded_subsystems & ~(degraded_gnss | degraded_storage | degraded_imu |
                                        degraded_touch | degraded_rtc)) != 0 ||
        record.source_accepted_fix_count + record.source_rejected_fix_count !=
            record.source_gnss_record_count ||
        record.source_event_record_count < record.lap_count) {
        return false;
    }
    if ((record.source_gnss_record_count > 0 || record.source_event_record_count > 0) &&
        record.source_last_record_sequence < record.source_first_record_sequence) {
        return false;
    }
    if ((record.logger_dropped_record_count > 0 || record.logger_write_failure_count > 0) &&
        record.integrity != SummaryIntegrity::partial_log) {
        return false;
    }
    if (record.lap_count == 0) {
        return record.best_lap_index == 0 &&
               record.best_lap_duration_ns == domain::kUnavailableTime;
    }
    return record.best_lap_index > 0 && record.best_lap_index <= record.lap_count &&
           record.best_lap_duration_ns > 0;
}

bool valid_summary_lap(const SummaryLapRecordV1& record) noexcept
{
    return record.schema_version == kLogFormatVersion &&
           record.record_size_bytes == sizeof(SummaryLapRecordV1) && record.lap_index > 0 &&
           record.lap_duration_ns > 0 && record.segment_sequence_1 > record.segment_sequence_0;
}

const char* fix_type_name(const domain::FixType value) noexcept
{
    switch (value) {
    case domain::FixType::no_fix:
        return "no_fix";
    case domain::FixType::dead_reckoning:
        return "dead_reckoning";
    case domain::FixType::fix_2d:
        return "fix_2d";
    case domain::FixType::fix_3d:
        return "fix_3d";
    case domain::FixType::gnss_dead_reckoning:
        return "gnss_dead_reckoning";
    case domain::FixType::time_only:
        return "time_only";
    }
    return "unknown";
}

const char* reject_reason_name(const domain::FixRejectReason value) noexcept
{
    switch (value) {
    case domain::FixRejectReason::none:
        return "none";
    case domain::FixRejectReason::invalid_status:
        return "invalid_status";
    case domain::FixRejectReason::stale:
        return "stale";
    case domain::FixRejectReason::non_monotonic_sequence:
        return "non_monotonic_sequence";
    case domain::FixRejectReason::non_monotonic_time:
        return "non_monotonic_time";
    case domain::FixRejectReason::excessive_accuracy:
        return "excessive_accuracy";
    case domain::FixRejectReason::implausible_motion:
        return "implausible_motion";
    }
    return "unknown";
}

const char* event_type_name(const SessionEventType value) noexcept
{
    switch (value) {
    case SessionEventType::session_started:
        return "session_started";
    case SessionEventType::lap_crossing:
        return "lap_crossing";
    case SessionEventType::overtime_started:
        return "overtime_started";
    case SessionEventType::session_stopped:
        return "session_stopped";
    case SessionEventType::rest_started:
        return "rest_started";
    case SessionEventType::rest_completed:
        return "rest_completed";
    case SessionEventType::diagnostic:
        return "diagnostic";
    }
    return "unknown";
}

}  // namespace track_timer::logger
