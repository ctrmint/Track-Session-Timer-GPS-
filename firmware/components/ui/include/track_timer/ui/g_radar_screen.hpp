#pragma once

#include "track_timer/ui/imu_meter.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

inline constexpr std::size_t kRadarRingCount = 3;
inline constexpr float kRadarFullScaleG = 1.5F;

// Radar-style G meter that owns the whole panel in G-Only mode.
//
// Polar rather than the cartesian scatter of GmeterScreen: concentric G rings, cardinal
// axes labelled for what the driver actually feels, a fading trace of recent samples, and
// the session maximum held where it occurred.
//
// Axis convention follows ImuMeterController: positive longitudinal is acceleration and
// positive lateral is to the right, so the marker moves the way the car is being pushed.
class GRadarScreen {
  public:
    explicit GRadarScreen(lv_obj_t* root) noexcept;

    void update(const ImuMeterSnapshot& snapshot) noexcept;

    [[nodiscard]] lv_obj_t* root() const noexcept;

    GRadarScreen(const GRadarScreen&) = delete;
    GRadarScreen& operator=(const GRadarScreen&) = delete;

  private:
    [[nodiscard]] lv_point_t plot(const PlanarAcceleration& point) const noexcept;

    lv_obj_t* root_{nullptr};
    std::array<lv_obj_t*, kRadarRingCount> rings_{};
    std::array<lv_obj_t*, kRadarRingCount> ring_labels_{};
    std::array<lv_obj_t*, 4> axis_labels_{};
    std::array<lv_obj_t*, 2> crosshairs_{};
    std::array<lv_obj_t*, kImuTrailCapacity> trail_{};
    lv_obj_t* current_{nullptr};
    lv_obj_t* peak_marker_{nullptr};
    lv_obj_t* status_{nullptr};
    std::array<lv_obj_t*, 5> peak_values_{};
};

}  // namespace track_timer::ui
