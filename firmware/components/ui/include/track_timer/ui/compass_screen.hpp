#pragma once

#include "track_timer/ui/compass.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// Compass mode's panel: a fixed card with north up, a marker that travels its rim, and the
// bearing in degrees at the centre.
//
// The card does not rotate, and that is a performance decision as much as a design one.
// This device draws its graphics the way g_radar_screen does - positioned styled objects,
// no canvas and no vector paths - so turning the card means repositioning every tick on
// every update. At 20 Hz that is the per-frame work that has starved the UI task twice
// (#133, #141). A fixed card is built once and never touched again; only three marker dots
// move, and north staying put is also what makes the display readable at a glance, since a
// frame of reference that rotates is not one.
//
// The numeral face carries no letters - its subset is `space - . 0-9 :` - so every word
// here is a built-in Montserrat label. The single "N" at the top is all the card needs to
// establish which way it is oriented; the bearing's own cardinal is spelled out under the
// degrees, where there is room for three characters.
class CompassScreen {
  public:
    explicit CompassScreen(lv_obj_t* root) noexcept;

    void update(const CompassViewModel& model) noexcept;

    CompassScreen(const CompassScreen&) = delete;
    CompassScreen& operator=(const CompassScreen&) = delete;

  private:
    // Sixteen ticks, one per cardinal point, so the quadrants read without labelling them.
    static constexpr std::size_t kTickCount = 16;
    // Three digits, or the three dashes that stand in for them. A bearing is always three
    // wide, so unlike Speed Only this block never has to reposition.
    static constexpr std::size_t kDegreeDigits = 3;
    // Growing outward, which is what makes a row of dots read as pointing somewhere.
    static constexpr std::size_t kMarkerDots = 3;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* cardinal_{nullptr};
    std::array<lv_obj_t*, kTickCount> ticks_{};
    std::array<lv_obj_t*, kMarkerDots> marker_{};
    std::array<lv_obj_t*, kDegreeDigits> digits_{};
    std::array<char, kDegreeDigits> digits_shown_{};
    std::array<char, kCompassStatusCapacity> status_shown_{};
    std::array<char, kCompassCardinalCapacity> cardinal_shown_{};
    // The bearing the marker is currently drawn at, so a heading that has not moved
    // enough to shift a dot by a pixel costs nothing.
    int marker_bearing_{-1};
    bool marker_visible_{false};
};

}  // namespace track_timer::ui
