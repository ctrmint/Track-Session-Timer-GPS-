#pragma once

#include "track_timer/ui/navigation_mode.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// Navigation mode's panel: road speed low and large, the receiver's clock top left, and an
// arrow top right pointing where the car is going, with north at twelve o'clock.
//
// Three departures from the screens it borrows from, each deliberate.
//
// The unit sits *beside* the speed rather than under it, because the digit line has moved
// to the bottom of the panel and there is nothing below it any more. Speed Only made the
// opposite call for a good reason - "beside would mean reserving width that the
// three-digit case needs, which is exactly the compromise that costs the face its size" -
// and that reason still holds: this face is 208 px against Speed Only's 248. Speed Only is
// not changed to match. It answers a different question, with the whole panel to answer it
// in.
//
// There is no title. The top left is the clock and the top right is the arrow, and a mode
// label would have to displace one of them to say what three readouts already say.
//
// The arrow is dots, not artwork. Rotating a bitmap or walking a vector path every tick at
// 20 Hz is the per-frame work that starved the UI task in #133 and #141, so this follows
// CompassScreen: a ring built once and never touched, and five dots that move only when
// the bearing has changed by a whole degree.
class NavigationScreen {
  public:
    explicit NavigationScreen(lv_obj_t* root) noexcept;

    void update(const NavigationViewModel& model) noexcept;

    NavigationScreen(const NavigationScreen&) = delete;
    NavigationScreen& operator=(const NavigationScreen&) = delete;

  private:
    // Three digits, or the three dashes that stand in for them.
    static constexpr std::size_t kSpeedDigits = 3;
    // "HH:MM:SS" - six digits and the two separators, each in its own fixed cell so a
    // minute rolling over cannot shuffle the hours sideways.
    static constexpr std::size_t kTimeCells = 8;
    // A tail, a pivot and three dots growing towards the tip. Fewer than three and it
    // reads as a dot rather than as pointing somewhere; the tail is what makes the
    // difference between an arrow and a comet.
    static constexpr std::size_t kArrowDots = 5;
    // Four cardinals and the four between them, which is as much as a 128 px ring can
    // carry without the ticks closing into a line.
    static constexpr std::size_t kTickCount = 8;

    lv_obj_t* root_{nullptr};
    std::array<lv_obj_t*, kTimeCells> time_cells_{};
    std::array<char, kTimeCells + 1> time_shown_{};
    lv_obj_t* time_status_{nullptr};
    std::array<char, kNavigationStatusCapacity> time_status_shown_{};
    std::array<lv_obj_t*, kSpeedDigits> digits_{};
    std::array<char, kSpeedDigits> digits_shown_{};
    lv_obj_t* unit_{nullptr};
    lv_obj_t* speed_status_{nullptr};
    std::array<char, kSpeedOnlyStatusCapacity> speed_status_shown_{};
    std::array<lv_obj_t*, kTickCount> ticks_{};
    std::array<lv_obj_t*, kArrowDots> arrow_{};
    lv_obj_t* heading_status_{nullptr};
    std::array<char, kCompassStatusCapacity> heading_status_shown_{};

    // Measured, not assumed. The face size is chosen against a compile-time budget for
    // this width, but where the label actually lands is decided by what the font renders,
    // so a metrics change moves the unit instead of overlapping the digits.
    std::int32_t unit_width_{0};
    // Where the digit block is centred: the middle of what is left after the unit has
    // taken its width from the right. The unit itself never moves, which is the point -
    // only the digits recentre as the reading crosses 9 to 10 and 99 to 100.
    std::int32_t digits_centre_{0};
    std::size_t shown_count_{0};
    bool speed_valid_{false};
    bool time_valid_{false};
    int arrow_bearing_{-1};
    bool arrow_visible_{false};
};

}  // namespace track_timer::ui
