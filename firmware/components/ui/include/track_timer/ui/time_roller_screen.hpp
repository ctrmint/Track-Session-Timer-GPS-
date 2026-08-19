#pragma once

#include "track_timer/ui/gesture_input.hpp"
#include "track_timer/ui/time_roller.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <lvgl.h>

#include <array>
#include <cstdint>

namespace track_timer::ui {

// Two columns of minutes and seconds, both live, rolled with the finger.
//
// The columns are targeted by where the touch starts rather than by a focus that has to be
// switched, so minutes and seconds are adjusted without a mode change between them.
//
// Digits sit in fixed cells, as UI_FOUNDATION requires: an auto-sizing label re-lays-out
// whenever the text changes, so the field would visibly shift while it rolled. Minutes are
// right-aligned in a field wide enough for the largest value the setting allows, with
// unused leading cells left blank rather than zero-padded - "1439" and "20" both belong in
// a duration, but "0020" does not read as a time.
class TimeRollerScreen {
  public:
    explicit TimeRollerScreen(lv_obj_t* root) noexcept;

    // Points the screen at a field and loads its current value.
    void configure(const char* title, SettingsField field, std::uint32_t seconds) noexcept;
    // Redraws from the roller's state.
    void refresh() noexcept;
    // Shows how far through a hold-to-save the finger is, 0 to 1. Without this a longer
    // hold reads as an unresponsive screen rather than a deliberate one.
    void set_hold_progress(float fraction) noexcept;

    [[nodiscard]] TimeRoller& roller() noexcept;
    // Which column a touch at this x belongs to. The split is the midpoint between them.
    [[nodiscard]] RollerColumn column_at(std::int16_t x) const noexcept;
    [[nodiscard]] lv_obj_t* root() const noexcept;

    TimeRollerScreen(const TimeRollerScreen&) = delete;
    TimeRollerScreen& operator=(const TimeRollerScreen&) = delete;

  private:
    static constexpr std::size_t kMaximumMinuteCells = 4;

    void build_cells(std::int32_t font_px, std::size_t minute_digits) noexcept;
    void write_column(bool minutes) noexcept;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* colon_{nullptr};
    lv_obj_t* minutes_unit_{nullptr};
    lv_obj_t* seconds_unit_{nullptr};
    lv_obj_t* hint_{nullptr};
    lv_obj_t* hold_bar_{nullptr};
    std::array<lv_obj_t*, kMaximumMinuteCells> minute_cells_{};
    std::array<lv_obj_t*, 2> second_cells_{};

    TimeRoller roller_{};
    SettingsField field_{SettingsField::average_lap};
    std::size_t minute_digits_{2};
    std::int32_t split_x_{300};
    std::int32_t font_px_{0};
};

}  // namespace track_timer::ui
