#include "track_timer/ui/trackday_screen.hpp"

#include "track_timer/ui/lvgl_visual_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
// 170 px, about 10 mm of numeral on this 311 PPI panel, against 3.9 mm for the largest
// built-in font. Viable only because LVGL now allocates from the ESP-IDF heap, so the
// glyph bitmaps live in PSRAM rather than a 64 KB pool.
//
// This is the largest size that fits: the widest digit in this face advances 689/1000 em,
// so four digits plus a colon need 541 px of the 600 px panel, leaving 30 px each side.
// The next step up overflows.
constexpr std::int32_t kCountdownPx = 170;

// Sized from the font's own metrics, not estimated. Cells are fixed at the widest
// numeral's advance plus a little side bearing, so a 1 occupies exactly the space an 8
// does and the time cannot shift as it counts down.
constexpr std::int32_t kDigitCellWidth = 123;
constexpr std::int32_t kColonCellWidth = 49;
constexpr std::int32_t kCellHeight = 207;
constexpr std::int32_t kCountdownTop = 48;

constexpr std::int32_t kBarMargin = 24;
constexpr std::int32_t kBarWidth = kScreenWidth - 2 * kBarMargin;
constexpr std::int32_t kBarHeight = 26;
constexpr std::int32_t kBarY = 372;

constexpr std::uint32_t kBackground = 0x000000;
constexpr std::uint32_t kMuted = 0x7C8899;
constexpr std::uint32_t kBarTrack = 0x1E252F;

}  // namespace

TrackdayScreen::TrackdayScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    track_label_ = create_label(root_, Typography::body, kMuted);
    lv_obj_align(track_label_, LV_ALIGN_TOP_MID, 0, 16);

    const auto* font = countdown_font(kCountdownPx);
    const auto total = kDigitCellWidth * 4 + kColonCellWidth;
    auto x = (kScreenWidth - total) / 2;
    for (std::size_t index = 0; index < cells_.size(); ++index) {
        const auto is_colon = index == 2;
        const auto width = is_colon ? kColonCellWidth : kDigitCellWidth;
        auto* cell = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(cell, font, 0);
        }
        // A fixed cell with centred text: the glyph may vary in width, the cell does not.
        lv_obj_set_size(cell, width, kCellHeight);
        lv_obj_set_pos(cell, x, kCountdownTop);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, is_colon ? ":" : "0");
        cells_[index] = cell;
        x += width;
    }

    laps_label_ = create_label(root_, Typography::timer_secondary, 0xFFFFFF);
    lv_obj_align(laps_label_, LV_ALIGN_TOP_MID, 0, 272);

    status_label_ = create_label(root_, Typography::caption, kMuted);
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_label_set_text(status_label_, mode_note_);

    bar_track_ = lv_obj_create(root_);
    lv_obj_remove_style_all(bar_track_);
    lv_obj_set_size(bar_track_, kBarWidth, kBarHeight);
    lv_obj_set_pos(bar_track_, kBarMargin, kBarY);
    lv_obj_set_style_bg_color(bar_track_, lv_color_hex(kBarTrack), 0);
    lv_obj_set_style_bg_opa(bar_track_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_track_, kBarHeight / 2, 0);
    lv_obj_remove_flag(bar_track_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bar_track_, LV_OBJ_FLAG_SCROLLABLE);

    // A plain sized object rather than lv_bar: the width is the only thing that changes,
    // and driving it directly keeps the redraw to one filled rectangle.
    bar_fill_ = lv_obj_create(bar_track_);
    lv_obj_remove_style_all(bar_fill_);
    lv_obj_set_size(bar_fill_, kBarWidth, kBarHeight);
    lv_obj_set_pos(bar_fill_, 0, 0);
    lv_obj_set_style_bg_opa(bar_fill_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar_fill_, kBarHeight / 2, 0);
    lv_obj_remove_flag(bar_fill_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bar_fill_, LV_OBJ_FLAG_SCROLLABLE);
}

void TrackdayScreen::update(const TrackdayModeViewModel& model) noexcept
{
    // Only write when the value actually changed. lv_label_set_text always copies and
    // invalidates, and at this glyph size a needless invalidation re-rasterises the whole
    // countdown - which is enough, at LVGL tick rate, to starve the UI task.
    // Per-cell comparison: only the digits that actually changed are rewritten, so a
    // ticking second touches one or two cells rather than re-rendering the whole time.
    if (std::strncmp(shown_countdown_.data(), model.countdown.data(),
                     shown_countdown_.size()) != 0) {
        std::snprintf(shown_countdown_.data(), shown_countdown_.size(), "%s",
                      model.countdown.data());
        const char* text = shown_countdown_.data();
        const auto length = std::strlen(text);
        for (std::size_t index = 0; index < cells_.size(); ++index) {
            const char glyph[2] = {index < length ? text[index] : ' ', '\0'};
            if (std::strcmp(lv_label_get_text(cells_[index]), glyph) != 0) {
                lv_label_set_text(cells_[index], glyph);
            }
        }
    }

    const auto colour = urgency_rgb(model.urgency);
    if (colour != shown_colour_) {
        shown_colour_ = colour;
        for (auto* cell : cells_) {
            lv_obj_set_style_text_color(cell, lv_color_hex(colour), 0);
        }
        lv_obj_set_style_bg_color(bar_fill_, lv_color_hex(colour), 0);
    }
    const auto ratio = std::clamp(model.remaining_ratio, 0.0F, 1.0F);
    const auto width = static_cast<std::int32_t>(std::lround(ratio * kBarWidth));
    // Zero width would leave a rounded stub, so an empty bar is hidden outright.
    if (width != shown_bar_width_) {
        shown_bar_width_ = width;
        if (width <= 0) {
            lv_obj_add_flag(bar_fill_, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_remove_flag(bar_fill_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_width(bar_fill_, width);
        }
    }

    if (std::strncmp(shown_laps_.data(), model.estimated_laps.data(),
                     shown_laps_.size()) == 0 && model.estimate_available) {
        return;
    }
    std::snprintf(shown_laps_.data(), shown_laps_.size(), "%s",
                  model.estimated_laps.data());
    lv_label_set_text(laps_label_, model.estimate_available
                                       ? model.estimated_laps.data()
                                       : "SET AVERAGE LAP FOR ESTIMATE");
    lv_obj_set_style_text_color(
        laps_label_, lv_color_hex(model.estimate_available ? 0xFFFFFF : kMuted), 0);

    lv_label_set_text(status_label_, model.urgency == SessionUrgency::overtime
                                         ? "OVERTIME"
                                         : mode_note_);
    lv_obj_set_style_text_color(
        status_label_,
        lv_color_hex(model.urgency == SessionUrgency::overtime ? colour : kMuted), 0);
}

void TrackdayScreen::set_mode_note(const char* const note) noexcept
{
    mode_note_ = note == nullptr ? "" : note;
    lv_label_set_text(status_label_, mode_note_);
}

void TrackdayScreen::set_track_name(const char* const name) noexcept
{
    lv_label_set_text(track_label_, name == nullptr ? "" : name);
}

lv_obj_t* TrackdayScreen::root() const noexcept { return root_; }
lv_obj_t* TrackdayScreen::bar() const noexcept { return bar_fill_; }

}  // namespace track_timer::ui
