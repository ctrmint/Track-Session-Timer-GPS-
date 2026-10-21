#pragma once

#include "track_timer/ui/presenter.hpp"

#include <lvgl.h>

#include <array>
#include <cstdint>

namespace track_timer::ui {

// The running-session screen for Race: the lap in progress, the fastest lap of the
// session, and the time left in it.
//
// Race used to borrow TrackdayScreen, which is a countdown and an estimated-laps figure
// and, as its own header says, has nowhere to put a lap time. The times were being
// computed every tick and formatted by ui::present, and then nothing drew them (#221).
//
// Separate from TrackdayScreen rather than lap fields added to it under a flag. Track Day
// withholding live lap times is the compliance rule #132 established, and a screen that
// has no lap labels at all cannot leak one through a mode check that is wrong.
//
// Fed the same DeviceViewModel that ui::present already produces, so the formatting -
// including "--:--.---" for a lap nothing has measured yet - is the one the host tests
// cover rather than a fourth copy of it.
class RaceScreen {
  public:
    explicit RaceScreen(lv_obj_t* root) noexcept;

    void update(const DeviceViewModel& model) noexcept;
    void set_track_name(const char* name) noexcept;

    [[nodiscard]] lv_obj_t* root() const noexcept;

    RaceScreen(const RaceScreen&) = delete;
    RaceScreen& operator=(const RaceScreen&) = delete;

  private:
    // One object per character, each in a cell of fixed width, as every large numeral on
    // this device is drawn: a 1 is narrower than an 8, so a single auto-sizing label would
    // let the reading twitch sideways as it climbs. UI_FOUNDATION.md requires it.
    //
    // m m : s s . s s s - nine cells, so a lap past ten minutes still reads correctly
    // rather than losing its leading digit.
    static constexpr std::size_t kLapCells = 9;
    // m m : s s
    static constexpr std::size_t kSessionCells = 5;
    // Long enough for the catalogue's longest name; a longer one is compared and drawn
    // truncated rather than rewritten every tick.
    static constexpr std::size_t kTrackNameCapacity = 48;

    lv_obj_t* root_{nullptr};
    lv_obj_t* track_label_{nullptr};
    lv_obj_t* mode_label_{nullptr};
    lv_obj_t* current_caption_{nullptr};
    lv_obj_t* fastest_caption_{nullptr};
    lv_obj_t* session_caption_{nullptr};
    std::array<lv_obj_t*, kLapCells> current_cells_{};
    std::array<lv_obj_t*, kLapCells> fastest_cells_{};
    std::array<lv_obj_t*, kSessionCells> session_cells_{};
    // What each row currently shows, so a cell is only rewritten when its character
    // changes. The current lap advances every fix and the other two almost never, and a
    // repaint is the most expensive thing this device does (#213).
    std::array<char, kLapCells + 1> shown_current_{};
    std::array<char, kLapCells + 1> shown_fastest_{};
    std::array<char, kSessionCells + 1> shown_session_{};
    // The same gate for the title, which the router hands over on every display tick
    // while the one thing it names - the armed track - cannot change during a session.
    // lv_label_set_text invalidates whether or not the text differs, so writing it
    // unconditionally was a repaint of the widest label on the screen 25 times a second.
    std::array<char, kTrackNameCapacity> shown_track_{};
};

}  // namespace track_timer::ui
