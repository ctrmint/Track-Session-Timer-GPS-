#include "track_timer/ui/acceleration_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kSideMargin = 30;

constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

// The countdown's size, shared rather than spent: four digits and a point come to the same
// width as Clock's HH:MM, so this costs no font-cache entry.
constexpr std::int32_t kTimePx = 170;
constexpr std::int32_t kDigitCellWidth = cell_width_for(kTimePx);
constexpr std::int32_t kPointWidth = kTimePx * 288 / 1000;
constexpr std::int32_t kDigitCellHeight = cell_height_for(kTimePx);
constexpr std::int32_t kBlockWidth = kDigitCellWidth * 4 + kPointWidth;
constexpr std::int32_t kBlockLeft = (kScreenWidth - kBlockWidth) / 2;
static_assert(kBlockWidth <= kScreenWidth - 2 * kSideMargin,
              "the reading must fit the panel at this size, margins included");

constexpr std::int32_t kTitleTop = 12;
constexpr std::int32_t kBlockTop = 86;
constexpr std::int32_t kTargetTop = kBlockTop + kDigitCellHeight + 10;

}  // namespace

AccelerationScreen::AccelerationScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    title_ = create_label(root_, Typography::body, color::text_secondary, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, kSideMargin, kTitleTop);
    lv_label_set_text(title_, "0-X");

    status_ = create_label(root_, Typography::body, color::text_secondary,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(status_, 300);
    lv_obj_set_pos(status_, kScreenWidth - 300 - kSideMargin, kTitleTop);
    lv_label_set_text(status_, "");

    const auto* font = countdown_font(kTimePx);
    auto x = kBlockLeft;
    // Two digits, the point, two digits. The point is created and forgotten: it is the one
    // glyph here that never changes.
    for (std::size_t index = 0; index < digits_.size() + 1; ++index) {
        const auto is_point = index == 2;
        auto* cell = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(cell, font, 0);
        }
        lv_obj_set_style_text_color(cell, lv_color_hex(color::text_primary), 0);
        // Fixed cells, as every large numeral on this device uses: a 1 occupies the space
        // an 8 does, so the reading cannot twitch sideways as it climbs.
        lv_obj_set_size(cell, is_point ? kPointWidth : kDigitCellWidth, kDigitCellHeight);
        lv_obj_set_pos(cell, x, kBlockTop);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, is_point ? "." : "");
        if (is_point) {
            point_ = cell;
        }
        else {
            digits_[index > 2 ? index - 1 : index] = cell;
        }
        x += is_point ? kPointWidth : kDigitCellWidth;
    }
    for (auto& shown : digits_shown_) {
        shown = '\0';
    }

    target_ = create_label(root_, Typography::timer_primary, color::text_secondary);
    lv_obj_set_width(target_, kScreenWidth);
    lv_obj_set_pos(target_, 0, kTargetTop);
    lv_label_set_text(target_, "");

    status_shown_[0] = '\0';
    target_shown_[0] = '\0';
}

void AccelerationScreen::update(const AccelerationViewModel& model) noexcept
{
    if (std::strcmp(status_shown_.data(), model.status.data()) != 0) {
        lv_label_set_text(status_, model.status.data());
        lv_obj_set_style_text_color(status_, lv_color_hex(model.status_rgb), 0);
        std::snprintf(status_shown_.data(), status_shown_.size(), "%s", model.status.data());
    }

    // The colour is the signal that a run finished, so it is applied to every cell
    // including the point - a green reading with a white point would read as a glitch.
    if (model.time_rgb != time_rgb_shown_) {
        time_rgb_shown_ = model.time_rgb;
        const auto colour = lv_color_hex(model.time_rgb);
        for (auto* cell : digits_) {
            lv_obj_set_style_text_color(cell, colour, 0);
        }
        if (point_ != nullptr) {
            lv_obj_set_style_text_color(point_, colour, 0);
        }
    }

    // Right-aligned into the cells either side of the point, so "6.71" leaves the leading
    // cell blank rather than padding with a zero that reads as part of the number, and
    // "12.34" fills it. The point never moves, which is what keeps the two stable against
    // each other as the reading crosses ten seconds.
    const auto length = std::strlen(model.time.data());
    const auto* const text = model.time.data();
    const auto* const point = static_cast<const char*>(std::memchr(text, '.', length));
    const auto whole = point != nullptr ? static_cast<std::size_t>(point - text) : length;
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        char character = '\0';
        if (index < 2) {
            // Tens and units, filled from the right of the whole part.
            const auto offset = 2 - index;
            if (whole >= offset) {
                character = text[whole - offset];
            }
        }
        else if (point != nullptr) {
            const auto decimal = index - 2;
            if (whole + 1 + decimal < length) {
                character = text[whole + 1 + decimal];
            }
        }
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char cell[2] = {character, '\0'};
        lv_label_set_text(digits_[index], cell);
    }

    if (std::strcmp(target_shown_.data(), model.target.data()) != 0) {
        lv_label_set_text(target_, model.target.data());
        std::snprintf(target_shown_.data(), target_shown_.size(), "%s", model.target.data());
    }
}

}  // namespace track_timer::ui
