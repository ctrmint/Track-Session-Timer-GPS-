#include "track_timer/logger/formats.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <type_traits>

namespace {

template <std::size_t Capacity>
void copy_text(std::array<char, Capacity>& target, const char* value)
{
    target.fill('\0');
    std::snprintf(target.data(), target.size(), "%s", value);
}

}  // namespace

int main()
{
    using namespace track_timer;

    settings::DeviceSettings settings{};
    settings.session_duration_minutes = 30;
    settings.rest_duration_minutes = 10;
    copy_text(settings.selected_track_id, "synthetic_test_loop");

    logger::SessionMetaV1 meta{};
    meta.record_size_bytes = sizeof(meta);
    copy_text(meta.session_id, "2026-08-13T140501Z-001");
    copy_text(meta.firmware_commit, "0123456789abcdef0123456789abcdef01234567");
    copy_text(meta.hardware_profile, "waveshare-esp32-s3-2.41b");
    copy_text(meta.gnss_profile, "ublox-20hz-v1");
    meta.gnss_update_rate_hz = 20;
    meta.reset_reason = logger::ResetReason::power_on;
    meta.track_schema_version = 1;
    copy_text(meta.track_id, "synthetic_test_loop");
    copy_text(meta.track_fingerprint, "0123456789abcdef");
    meta.start_utc_ns = 1'700'000'000'000'000'000LL;
    meta.end_utc_ns = meta.start_utc_ns + 1'900'000'000'000LL;
    meta.settings = logger::make_session_settings(settings);
    assert(logger::valid_meta(meta));

    auto invalid_meta = meta;
    invalid_meta.schema_version = 2;
    assert(!logger::valid_meta(invalid_meta));
    invalid_meta = meta;
    copy_text(invalid_meta.track_fingerprint, "too-short");
    assert(!logger::valid_meta(invalid_meta));

    const domain::GnssFix accepted_fix{
        1'700'000'001'000'000'000LL,
        2'000'000,
        52.0,
        -1.0,
        100.0F,
        30.0F,
        90.0F,
        0.8F,
        0.1F,
        1.0F,
        41,
        0x07,
        18,
        domain::FixType::fix_3d,
        domain::FixRejectReason::none,
        true,
    };
    const auto accepted_record = logger::make_gnss_record(accepted_fix, 100);
    assert(logger::valid_gnss_record(accepted_record));
    assert(accepted_record.fix_sequence == 41);
    assert(std::strcmp(logger::fix_type_name(accepted_record.fix_type), "fix_3d") == 0);

    auto rejected_fix = accepted_fix;
    rejected_fix.sequence_number = 42;
    rejected_fix.accepted_for_timing = false;
    rejected_fix.reject_reason = domain::FixRejectReason::stale;
    const auto rejected_record = logger::make_gnss_record(rejected_fix, 101);
    assert(logger::valid_gnss_record(rejected_record));
    assert(std::strcmp(logger::reject_reason_name(rejected_record.reject_reason), "stale") == 0);

    auto unexplained_rejection = rejected_record;
    unexplained_rejection.reject_reason = domain::FixRejectReason::none;
    assert(!logger::valid_gnss_record(unexplained_rejection));

    const domain::LapEvent lap{3, 1'700'000'061'000'000'000LL, 61'250'000'000LL,
                               0.375, 41, 42, 0x05};
    const auto event = logger::make_lap_event_record(lap, 102, 63'000'000, 125'000, 5'000);
    assert(logger::valid_event_record(event));
    assert(event.segment_sequence_0 == accepted_record.fix_sequence);
    assert(event.segment_sequence_1 == rejected_record.fix_sequence);
    assert(std::strcmp(logger::event_type_name(event.event_type), "lap_crossing") == 0);

    const auto summary_lap = logger::make_summary_lap_record(event);
    assert(logger::valid_summary_lap(summary_lap));
    assert(summary_lap.event_record_sequence == event.record_sequence);

    auto untraceable_event = event;
    untraceable_event.segment_sequence_1 = untraceable_event.segment_sequence_0;
    assert(!logger::valid_event_record(untraceable_event));

    logger::SessionSummaryV1 summary{};
    summary.record_size_bytes = sizeof(summary);
    copy_text(summary.session_id, "2026-08-13T140501Z-001");
    summary.session_duration_ms = 125'000;
    summary.session_overrun_ms = 5'000;
    summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    summary.lap_count = 3;
    summary.best_lap_index = 3;
    summary.best_lap_duration_ns = lap.lap_duration_ns;
    summary.source_first_record_sequence = 1;
    summary.source_last_record_sequence = 102;
    summary.source_gnss_record_count = 2;
    summary.source_accepted_fix_count = 1;
    summary.source_rejected_fix_count = 1;
    summary.source_event_record_count = 4;
    assert(logger::valid_summary(summary));

    auto incomplete_summary = summary;
    incomplete_summary.logger_dropped_record_count = 1;
    assert(!logger::valid_summary(incomplete_summary));
    incomplete_summary.integrity = logger::SummaryIntegrity::partial_log;
    assert(logger::valid_summary(incomplete_summary));

    logger::SessionSummaryV1 empty_summary{};
    empty_summary.record_size_bytes = sizeof(empty_summary);
    copy_text(empty_summary.session_id, "2026-08-13T150000Z-002");
    empty_summary.session_duration_ms = 60'000;
    empty_summary.completion_reason = logger::SessionCompletionReason::driver_stop;
    assert(logger::valid_summary(empty_summary));

    assert(logger::kGnssCsvHeaderV1.find("accepted_for_timing,reject_reason") !=
           std::string_view::npos);
    assert(logger::kEventsCsvHeaderV1.find("segment_sequence_0,segment_sequence_1") !=
           std::string_view::npos);
    assert(logger::kSummaryLapsCsvHeaderV1.find("event_record_sequence") !=
           std::string_view::npos);
    assert(std::is_trivially_copyable_v<logger::SessionSummaryV1>);

    std::cout << "Versioned log formats and lap traceability passed\n";
    return 0;
}
