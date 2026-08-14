#include "track_timer/track/capture.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

track_timer::domain::GnssFix stationary_fix()
{
    track_timer::domain::GnssFix fix{};
    fix.measurement_time_ns = 1'000'000'000;
    fix.latitude_deg = 52.0;
    fix.longitude_deg = -1.0;
    fix.speed_mps = 0.1F;
    fix.horizontal_accuracy_m = 0.8F;
    fix.sequence_number = 42;
    fix.fix_type = track_timer::domain::FixType::fix_3d;
    fix.reject_reason = track_timer::domain::FixRejectReason::none;
    fix.accepted_for_timing = true;
    return fix;
}

}  // namespace

int main()
{
    using namespace track_timer::track;

    GateCaptureRequest request{};
    request.expected_heading_deg = 90.0;
    request.width_m = 20.0;
    GateCapturePreview preview{};
    assert(capture_stationary_gate(stationary_fix(), request, false, preview) ==
           GateCaptureResult::captured);
    assert(std::abs(preview.center.latitude_deg - 52.0) < 1.0e-9);
    assert(std::abs(preview.width_m - 20.0) < 1.0e-9);
    assert(std::abs(preview.gate.local_left.east_m) < 1.0e-9);
    assert(std::abs(preview.gate.local_left.north_m - 10.0) < 1.0e-9);
    assert(std::abs(preview.gate.local_right.north_m + 10.0) < 1.0e-9);
    assert(preview.gate.left.latitude_deg > preview.center.latitude_deg);
    assert(preview.gate.right.latitude_deg < preview.center.latitude_deg);
    assert(preview.source_fix_sequence == 42);

    const auto retained = preview;
    auto moving = stationary_fix();
    moving.speed_mps = 0.51F;
    assert(capture_stationary_gate(moving, request, false, preview) ==
           GateCaptureResult::moving);
    assert(preview.source_fix_sequence == retained.source_fix_sequence);

    assert(capture_stationary_gate(stationary_fix(), request, true, preview) ==
           GateCaptureResult::active_session);
    auto poor = stationary_fix();
    poor.horizontal_accuracy_m = 5.1F;
    assert(capture_stationary_gate(poor, request, false, preview) ==
           GateCaptureResult::poor_accuracy);
    auto rejected = stationary_fix();
    rejected.accepted_for_timing = false;
    assert(capture_stationary_gate(rejected, request, false, preview) ==
           GateCaptureResult::unusable_fix);
    auto negative_speed = stationary_fix();
    negative_speed.speed_mps = -0.1F;
    assert(capture_stationary_gate(negative_speed, request, false, preview) ==
           GateCaptureResult::unusable_fix);
    auto invalid_fix = stationary_fix();
    invalid_fix.latitude_deg = std::numeric_limits<double>::quiet_NaN();
    assert(capture_stationary_gate(invalid_fix, request, false, preview) ==
           GateCaptureResult::unusable_fix);

    auto invalid_request = request;
    invalid_request.width_m = 0.5;
    assert(capture_stationary_gate(stationary_fix(), invalid_request, false, preview) ==
           GateCaptureResult::invalid_request);
    invalid_request = request;
    invalid_request.expected_heading_deg = 360.0;
    assert(capture_stationary_gate(stationary_fix(), invalid_request, false, preview) ==
           GateCaptureResult::invalid_request);

    for (const auto result : {GateCaptureResult::captured,
                              GateCaptureResult::active_session,
                              GateCaptureResult::moving,
                              GateCaptureResult::unusable_fix,
                              GateCaptureResult::poor_accuracy,
                              GateCaptureResult::invalid_request,
                              GateCaptureResult::projection_failed}) {
        assert(std::strlen(gate_capture_result_name(result)) > 0);
    }

    std::cout << "Stationary gate capture, perpendicular preview, and safety locks passed\n";
    return 0;
}
