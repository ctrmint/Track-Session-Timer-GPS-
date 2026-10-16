#include "track_timer/ui/speed_only_screen.hpp"

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
constexpr std::int32_t kScreenCentre = kScreenWidth / 2;

// The same metrics the countdown, the time roller and Clock all take from the face itself
// rather than from a screenshot.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

// The largest size at which three digits fit between the margins:
// 540 / (3 x 0.7235) = 248.8.
constexpr std::int32_t kSpeedPx = 248;
constexpr std::int32_t kDigitCellWidth = cell_width_for(kSpeedPx);
constexpr std::int32_t kDigitCellHeight = cell_height_for(kSpeedPx);
static_assert(kDigitCellWidth * 3 <= kScreenWidth - 2 * kSideMargin,
              "three digits must fit the panel at this size, margins included");

constexpr std::int32_t kTitleTop = 10;
constexpr std::int32_t kDigitsTop = 52;
constexpr std::int32_t kUnitTop = kDigitsTop + kDigitCellHeight + 6;
static_assert(kUnitTop + 60 <= kScreenHeight, "the unit must fit under the numeral");

// Where a block of `count` cells starts so that it is centred on the panel.
constexpr std::int32_t block_left_for(const std::size_t count) noexcept
{
    return kScreenCentre - static_cast<std::int32_t>(count) * kDigitCellWidth / 2;
}

}  // namespace

SpeedOnlyScreen::SpeedOnlyScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    title_ = create_label(root_, Typography::body, color::text_secondary, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, kSideMargin, kTitleTop);
    lv_label_set_text(title_, "SPEED");

    status_ = create_label(root_, Typography::body, color::text_secondary,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(status_, 300);
    lv_obj_set_pos(status_, kScreenWidth - 300 - kSideMargin, kTitleTop);
    lv_label_set_text(status_, "");

    const auto* font = countdown_font(kSpeedPx);
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        auto* cell = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(cell, font, 0);
        }
        lv_obj_set_style_text_color(cell, lv_color_hex(color::text_primary), 0);
        // Fixed cells, as every other large numeral on this device uses: a 1 occupies the
        // space an 8 does, so no digit shuffles against its neighbour as the speed changes.
        // Only the block as a whole moves, and only when the digit count does.
        lv_obj_set_size(cell, kDigitCellWidth, kDigitCellHeight);
        lv_obj_set_pos(cell, block_left_for(digits_.size()) +
                                 static_cast<std::int32_t>(index) * kDigitCellWidth,
                       kDigitsTop);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, "");
        digits_[index] = cell;
        digits_shown_[index] = '\0';
    }
    shown_count_ = digits_.size();

    // Under the numeral rather than beside it. Beside would mean reserving width that the
    // three-digit case needs, which is exactly the compromise that costs the face its size.
    unit_ = create_label(root_, Typography::timer_primary, color::text_secondary);
    lv_obj_set_width(unit_, kScreenWidth);
    lv_obj_set_pos(unit_, 0, kUnitTop);
    lv_label_set_text(unit_, "");

    status_shown_[0] = '\0';
}

void SpeedOnlyScreen::update(const SpeedOnlyViewModel& model) noexcept
{
    // Only what changed is written. lv_label_set_text always copies and invalidates, and
    // at 248 px an invalidation re-rasterises the largest glyph on the device; road speed
    // changes continuously, so rewriting every cell each tick would do that work for every
    // fix the receiver sends.
    if (std::strcmp(status_shown_.data(), model.status.data()) != 0) {
        const auto fixed = model.speed_valid;
        // The unit sits with the number and the fault sits in the corner, so the two are
        // never on screen together: "MPH" under a row of dashes would read as a speed of
        // nothing rather than as no speed at all.
        lv_label_set_text(unit_, fixed ? model.status.data() : "");
        lv_label_set_text(status_, fixed ? "" : model.status.data());
        lv_obj_set_style_text_color(status_, lv_color_hex(model.status_rgb), 0);
        for (auto* cell : digits_) {
            lv_obj_set_style_text_color(
                cell, lv_color_hex(fixed ? color::text_primary : model.status_rgb), 0);
        }
        std::snprintf(status_shown_.data(), status_shown_.size(), "%s", model.status.data());
    }

    const auto length = std::strlen(model.speed.data());
    const auto count = length > kSpeedDigits ? kSpeedDigits : length;
    if (count != shown_count_) {
        // The digit count changed, so the block recentres. This happens twice across the
        // whole speed range - at 9 to 10 and at 99 to 100 - and never while a reading sits
        // inside one of those bands.
        shown_count_ = count;
        const auto left = block_left_for(count);
        for (std::size_t index = 0; index < digits_.size(); ++index) {
            lv_obj_set_pos(digits_[index],
                           left + static_cast<std::int32_t>(index) * kDigitCellWidth,
                           kDigitsTop);
        }
    }

    // Left-aligned into the leading cells, because the block has already been positioned
    // for exactly this many digits. Any cell beyond the count is blanked rather than left
    // holding the previous reading's digit.
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        const char character = index < count ? model.speed[index] : '\0';
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(digits_[index], text);
    }
}

}  // namespace track_timer::ui
