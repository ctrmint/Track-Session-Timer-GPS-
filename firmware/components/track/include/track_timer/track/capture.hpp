#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/track/definition.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::track {

inline constexpr double kMinimumCapturedGateWidthM = 1.0;
inline constexpr double kMaximumCapturedGateWidthM = 1'000.0;
inline constexpr float kMaximumCaptureSpeedMps = 0.5F;
inline constexpr float kMaximumCaptureHorizontalAccuracyM = 5.0F;

enum class GateCaptureResult : std::uint8_t {
    captured,
    active_session,
    moving,
    unusable_fix,
    poor_accuracy,
    invalid_request,
    projection_failed,
};

struct GateCaptureRequest {
    double expected_heading_deg{0.0};
    double width_m{20.0};
    double heading_tolerance_deg{60.0};
    double minimum_crossing_speed_mps{1.0};
    double rearm_corridor_m{10.0};
};

struct GateCapturePreview {
    GeographicPoint center{};
    DirectedGateDefinition gate{};
    double width_m{0.0};
    float source_horizontal_accuracy_m{0.0F};
    std::uint32_t source_fix_sequence{0};
};

// Captures a line centre from an accepted stationary fix. Expected direction is
// supplied by the user because a GNSS course-over-ground is unreliable at rest.
// Output remains unchanged when capture is rejected.
[[nodiscard]] GateCaptureResult capture_stationary_gate(
    const domain::GnssFix& fix, const GateCaptureRequest& request,
    bool session_active, GateCapturePreview& output) noexcept;

[[nodiscard]] const char* gate_capture_result_name(GateCaptureResult result) noexcept;

static_assert(std::is_trivially_copyable_v<GateCaptureRequest>);
static_assert(std::is_trivially_copyable_v<GateCapturePreview>);

}  // namespace track_timer::track
