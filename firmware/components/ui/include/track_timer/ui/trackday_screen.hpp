#pragma once

#include "track_timer/ui/active_session.hpp"
#include "track_timer/ui/timer_font.hpp"


#include <lvgl.h>

#include <array>
#include <cstdint>

namespace track_timer::ui {

// The running-session screen for Track Day: a countdown that dominates the panel, the
// estimated laps left, and a bar that decays with the session.
//
// Track Day deliberately shows no lap times while running. Many track days run under
// regulations that prohibit timing on circuit, so laps are recorded and held back for
// Review rather than displayed. That rule lives in ui::visibility_for; this screen simply
// has nowhere to put a lap time.
//
// GNSS retrofit: the laps estimate is derived from the configured average lap today. When
// a receiver exists it can be driven by measured laps instead, which changes only the
// value handed to update() - the screen needs no knowledge of where the number came from.
class TrackdayScreen {
  public:
    explicit TrackdayScreen(lv_obj_t* root) noexcept;

    void update(const TrackdayModeViewModel& model) noexcept;
    void set_track_name(const char* name) noexcept;

    // What the footer says about this mode. Race and G-Only have no running-session
    // layout of their own yet, so they borrow the countdown and say so rather than
    // presenting themselves as Track Day.
    void set_mode_note(const char* note) noexcept;

    [[nodiscard]] lv_obj_t* root() const noexcept;
    [[nodiscard]] lv_obj_t* bar() const noexcept;

    TrackdayScreen(const TrackdayScreen&) = delete;
    TrackdayScreen& operator=(const TrackdayScreen&) = delete;

  private:
    lv_obj_t* root_{nullptr};
    lv_obj_t* track_label_{nullptr};
    lv_obj_t* laps_label_{nullptr};
    lv_obj_t* status_label_{nullptr};
    const char* mode_note_{"TRACK DAY  -  lap times available in Review"};
    lv_obj_t* bar_track_{nullptr};
    lv_obj_t* bar_fill_{nullptr};
    // One object per character of MM:SS, each in a cell of fixed width. UI_FOUNDATION.md
    // requires fixed-cell timer fields so that changing numerals cannot move the time,
    // which a single auto-sizing label cannot honour: a 1 is narrower than an 8.
    std::array<lv_obj_t*, 5> cells_{};
    std::array<char, 32> shown_countdown_{};
    std::array<char, 24> shown_laps_{};
    std::uint32_t shown_colour_{0};
    std::int32_t shown_bar_width_{-1};
};

}  // namespace track_timer::ui
