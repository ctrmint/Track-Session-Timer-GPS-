#pragma once

#include "track_timer/ui/speed_only.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// Speed Only mode's panel: road speed as large as the panel can hold, and nothing else.
//
// 248 px of numeral, which is the arithmetic ceiling rather than a preference: the face's
// own metrics put a digit cell at 0.7235 of the point size, three digits need 2.1705 of
// it, and 540 px across a 600 px panel with margins divides to 248. That is 302 px of
// glyph height - two thirds of the panel - against Clock's 207.
//
// Reaching it needs the block to be centred on the digits actually shown rather than
// right-aligned into three fixed cells. The fixed-cell layout looks like the safer choice
// and is not: with three cells pinned, only a two-digit reading can be centred, and both
// `0` and `140` end up 67 px off to one side. `0` is the reading on the panel whenever the
// car is standing still, which is most of the time anyone is looking at it.
//
// Centring the block instead costs one thing and buys two. The cost is that the number
// repositions as it crosses 9 to 10 and 99 to 100. The gains are that every reading is
// centred and that the face can be 33% larger, because nothing has to be held in reserve
// for a hundreds digit that is usually absent. Two repositions across the whole speed
// range is not jitter - the cells stay fixed width, so no digit ever shuffles against its
// neighbour, which is the twitch that fixed cells exist to prevent.
class SpeedOnlyScreen {
  public:
    explicit SpeedOnlyScreen(lv_obj_t* root) noexcept;

    void update(const SpeedOnlyViewModel& model) noexcept;

    SpeedOnlyScreen(const SpeedOnlyScreen&) = delete;
    SpeedOnlyScreen& operator=(const SpeedOnlyScreen&) = delete;

  private:
    // Three digits, or the three dashes that stand in for them.
    static constexpr std::size_t kSpeedDigits = 3;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* unit_{nullptr};
    std::array<lv_obj_t*, kSpeedDigits> digits_{};
    std::array<char, kSpeedDigits> digits_shown_{};
    std::array<char, kSpeedOnlyStatusCapacity> status_shown_{};
    // How many cells were on screen last time. The block only moves when this changes, so
    // a speed climbing within one digit count never repositions anything.
    std::size_t shown_count_{0};
};

}  // namespace track_timer::ui
