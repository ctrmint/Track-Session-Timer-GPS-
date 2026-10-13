#include "track_timer/ui/clock_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kScreenHeight = 450;
constexpr std::int32_t kSideMargin = 30;

// The same face and sizes the countdown and the time roller take their metrics from,
// read off the face itself rather than measured from a screenshot: the widest numeral
// advances 0.7235 of the point size, the colon 0.288, and a line is 1.218.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

constexpr std::int32_t kClockPx = 170;
constexpr std::int32_t kSecondsPx = 100;

constexpr std::int32_t kDigitCellWidth = cell_width_for(kClockPx);
constexpr std::int32_t kColonWidth = kClockPx * 288 / 1000;
constexpr std::int32_t kDigitCellHeight = cell_height_for(kClockPx);
constexpr std::int32_t kBlockWidth = kDigitCellWidth * 4 + kColonWidth;
constexpr std::int32_t kBlockLeft = (kScreenWidth - kBlockWidth) / 2;
static_assert(kBlockWidth <= kScreenWidth - 2 * kSideMargin,
              "the hour and minute must fit the panel at this size, margins included");

constexpr std::int32_t kSecondsCellWidth = cell_width_for(kSecondsPx);
constexpr std::int32_t kSecondsCellHeight = cell_height_for(kSecondsPx);
constexpr std::int32_t kSecondsBlockWidth = kSecondsCellWidth * 2;
// Flush with the minute's right edge, so the footer hangs off the block above it rather
// than off the panel.
constexpr std::int32_t kSecondsLeft = kBlockLeft + kBlockWidth - kSecondsBlockWidth;

constexpr std::int32_t kTitleTop = 12;
constexpr std::int32_t kBlockTop = 58;
constexpr std::int32_t kSecondsTop = kBlockTop + kDigitCellHeight + 8;
// Roughly centred against the seconds beside it. A single line of heading text is about
// 29 px, which is near enough to split the difference without measuring at runtime.
constexpr std::int32_t kDateTop = kSecondsTop + (kSecondsCellHeight - 29) / 2;
constexpr std::int32_t kDateWidth = 260;
static_assert(kSecondsTop + kSecondsCellHeight <= kScreenHeight,
              "the footer must fit above the bottom of the panel");
static_assert(kSideMargin + kDateWidth <= kSecondsLeft,
              "the date must not run into the seconds");

// Fixed cells throughout. A 1 occupies the space an 8 does, so neither the minute nor the
// second shuffles the face sideways as it changes - which on a clock would be a visible
// twitch once a second, for as long as the device is on.
lv_obj_t* make_cell(lv_obj_t* const parent, const lv_font_t* const font,
                    const std::int32_t width, const std::int32_t height,
                    const std::int32_t x, const std::int32_t y, const char* const text,
                    const std::uint32_t rgb) noexcept
{
    auto* cell = lv_label_create(parent);
    if (font != nullptr) {
        lv_obj_set_style_text_font(cell, font, 0);
    }
    lv_obj_set_style_text_color(cell, lv_color_hex(rgb), 0);
    lv_obj_set_size(cell, width, height);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(cell, text);
    return cell;
}

}  // namespace

ClockScreen::ClockScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    title_ = create_label(root_, Typography::body, color::text_secondary, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, kSideMargin, kTitleTop);
    lv_label_set_text(title_, "CLOCK");

    status_ = create_label(root_, Typography::body, color::text_secondary,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(status_, 300);
    lv_obj_set_pos(status_, kScreenWidth - 300 - kSideMargin, kTitleTop);
    lv_label_set_text(status_, "");

    const auto* font = countdown_font(kClockPx);
    auto x = kBlockLeft;
    // Hour, hour, colon, minute, minute. The colon is created and forgotten rather than
    // held: it is the one glyph on this screen that can never change.
    for (std::size_t index = 0; index < digits_.size() + 1; ++index) {
        const auto is_colon = index == 2;
        auto* cell = make_cell(root_, font, is_colon ? kColonWidth : kDigitCellWidth,
                               kDigitCellHeight, x, kBlockTop, is_colon ? ":" : "",
                               color::text_primary);
        if (!is_colon) {
            digits_[index > 2 ? index - 1 : index] = cell;
        }
        x += is_colon ? kColonWidth : kDigitCellWidth;
    }
    for (auto& shown : digits_shown_) {
        shown = '\0';
    }

    const auto* seconds_font = countdown_font(kSecondsPx);
    x = kSecondsLeft;
    for (auto& cell : seconds_) {
        cell = make_cell(root_, seconds_font, kSecondsCellWidth, kSecondsCellHeight, x,
                         kSecondsTop, "", color::text_secondary);
        x += kSecondsCellWidth;
    }
    for (auto& shown : seconds_shown_) {
        shown = '\0';
    }

    date_ = create_label(root_, Typography::heading, color::text_secondary,
                         LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(date_, kDateWidth);
    lv_obj_set_pos(date_, kSideMargin, kDateTop);
    lv_label_set_text(date_, "");

    status_shown_[0] = '\0';
    date_shown_[0] = '\0';
}

void ClockScreen::update(const ClockViewModel& model) noexcept
{
    // Only what changed is written. lv_label_set_text always copies and invalidates, and
    // at 170 px an invalidation re-rasterises a glyph; a clock redraws every second for as
    // long as the device is on, so rewriting all seven cells each time would be doing that
    // work thousands of times an hour to produce the same pixels.
    if (std::strcmp(status_shown_.data(), model.status.data()) != 0) {
        lv_label_set_text(status_, model.status.data());
        lv_obj_set_style_text_color(status_, lv_color_hex(model.status_rgb), 0);
        std::snprintf(status_shown_.data(), status_shown_.size(), "%s", model.status.data());
        // The seconds take the state's colour when there is no time, so the dashes read as
        // the same fault the status line names rather than as a separate one.
        const auto seconds_rgb = model.digits_valid ? color::text_secondary : model.status_rgb;
        for (auto* cell : seconds_) {
            lv_obj_set_style_text_color(cell, lv_color_hex(seconds_rgb), 0);
        }
    }

    // The view model's placeholder is "--:--" and "--", and the large face can spell both:
    // its subset carries the separators as well as the digits. So the dashes go in the
    // cells the digits use, and the layout does not move between states - a face that
    // jumped position when the receiver dropped out would read as a different fault again.
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        // "HH:MM" - the colon at position 2 is the cell that is not in digits_, so the
        // minute reads one further along the string than its cell index.
        const char character = model.time[index < 2 ? index : index + 1];
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(digits_[index], text);
    }

    for (std::size_t index = 0; index < seconds_.size(); ++index) {
        const char character = model.seconds[index];
        if (character == seconds_shown_[index]) {
            continue;
        }
        seconds_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(seconds_[index], text);
    }

    if (std::strcmp(date_shown_.data(), model.date.data()) != 0) {
        lv_label_set_text(date_, model.date.data());
        std::snprintf(date_shown_.data(), date_shown_.size(), "%s", model.date.data());
    }
}

}  // namespace track_timer::ui
