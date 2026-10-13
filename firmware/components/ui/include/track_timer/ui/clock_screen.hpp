#pragma once

#include "track_timer/ui/clock.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// Clock mode's panel: the hour and minute as large as the width allows, the seconds large
// underneath them, and the date and time source along the same footer.
//
// Why the hour and minute dominate rather than one line of HH:MM:SS. The large face's own
// metrics put a digit cell at 0.7235 of the point size and a colon at 0.288, and there are
// 540 px to spend across a 600 px panel with margins. Eight glyphs - six digits and two
// colons - cap the face at 112 px, barely a third of the panel's height. Five glyphs cap
// it at 173, so 170 px fits and yields half again as much glyph. Splitting the seconds
// onto their own line then spends the height that buys: the hour and minute at 170 px and
// the seconds at 100 px fill the panel from the title down, which one line of 112 px
// never could.
//
// It is also how a clock is read. The minute is the information; the second is the
// reassurance that the reading is live, and does not have to be the same size to give it.
//
// 170 px is the size the countdown already uses, which matters more than it looks:
// countdown_font() caches a fixed number of rasterised sizes and returns nothing once it
// is full, at which point the label silently keeps the 14 px default. Sharing a size costs
// no cache entry at all; the seconds cost the one remaining headroom was raised for.
class ClockScreen {
  public:
    explicit ClockScreen(lv_obj_t* root) noexcept;

    void update(const ClockViewModel& model) noexcept;

    ClockScreen(const ClockScreen&) = delete;
    ClockScreen& operator=(const ClockScreen&) = delete;

  private:
    // "HH" and "MM". The colon between them never changes, so it is not one of these.
    static constexpr std::size_t kClockDigits = 4;
    static constexpr std::size_t kSecondsDigits = 2;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* date_{nullptr};
    std::array<lv_obj_t*, kClockDigits> digits_{};
    std::array<char, kClockDigits> digits_shown_{};
    std::array<lv_obj_t*, kSecondsDigits> seconds_{};
    std::array<char, kSecondsDigits> seconds_shown_{};
    std::array<char, kClockDateCapacity> date_shown_{};
    std::array<char, kClockStatusCapacity> status_shown_{};
};

}  // namespace track_timer::ui
