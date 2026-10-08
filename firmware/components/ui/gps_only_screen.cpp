#include "track_timer/ui/gps_only_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;

// 120 px, about 7 mm of numeral on this 311 PPI panel. Smaller than the countdown's
// 170 px because three digits and a unit have to share the width, and because this is
// read in glances rather than watched.
constexpr std::int32_t kSpeedPx = 120;

// Scaled from the countdown's measured cell, which was taken from the face's own metrics
// rather than estimated: 123/170 of the point size for the widest numeral's advance plus
// side bearing, and 207/170 for the line.
constexpr std::int32_t kDigitCellWidth = kSpeedPx * 123 / 170;
constexpr std::int32_t kDigitCellHeight = kSpeedPx * 207 / 170;
constexpr std::int32_t kSpeedTop = 44;
constexpr std::int32_t kSpeedBlockWidth = kDigitCellWidth * 3;
constexpr std::int32_t kSpeedLeft = (kScreenWidth - kSpeedBlockWidth) / 2;

// Two columns: where the car is on the left, how well the receiver knows it on the right.
constexpr std::int32_t kColumnWidth = 230;
constexpr std::int32_t kLeftColumn = 24;
constexpr std::int32_t kRightColumn = kScreenWidth - kColumnWidth - 24;
constexpr std::int32_t kGridTop = 232;
constexpr std::int32_t kRowPitch = 43;
constexpr std::size_t kRowsPerColumn = kGpsOnlyRowCount / 2;

}  // namespace

GpsOnlyScreen::GpsOnlyScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    title_ = create_label(root_, Typography::body, color::text_secondary,
                          LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, kLeftColumn, 12);
    lv_label_set_text(title_, "GPS ONLY");

    status_ = create_label(root_, Typography::body, color::text_secondary,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(status_, 300);
    lv_obj_set_pos(status_, kScreenWidth - 300 - kLeftColumn, 12);
    lv_label_set_text(status_, "");

    const auto* font = countdown_font(kSpeedPx);
    auto x = kSpeedLeft;
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        auto* cell = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(cell, font, 0);
        }
        lv_obj_set_style_text_color(cell, lv_color_hex(color::text_primary), 0);
        // Fixed cells, as the countdown uses: a 1 occupies the space an 8 does, so the
        // reading cannot jitter sideways as the speed changes.
        lv_obj_set_size(cell, kDigitCellWidth, kDigitCellHeight);
        lv_obj_set_pos(cell, x, kSpeedTop);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, "");
        digits_[index] = cell;
        digits_shown_[index] = '\0';
        x += kDigitCellWidth;
    }

    unit_ = create_label(root_, Typography::heading, color::text_secondary,
                         LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(unit_, kSpeedLeft + kSpeedBlockWidth + 10, kSpeedTop + kSpeedPx - 34);
    lv_label_set_text(unit_, "MPH");

    secondary_ = create_label(root_, Typography::body, color::text_secondary);
    lv_obj_set_width(secondary_, kScreenWidth);
    lv_obj_set_pos(secondary_, 0, kSpeedTop + kDigitCellHeight + 2);
    lv_label_set_text(secondary_, "");

    // Shown instead of the numeral whenever there is no speed to show. The numeral face
    // cannot spell a placeholder - it holds only digits and separators - and a zero would
    // be a lie, so this carries the same dashes the unknown rows use, at reading size and
    // in the state's colour. The state is named once, in the status line.
    placeholder_ = create_label(root_, Typography::timer_primary, color::text_secondary);
    lv_obj_set_width(placeholder_, kScreenWidth);
    lv_obj_set_pos(placeholder_, 0, kSpeedTop + kDigitCellHeight / 2 - 30);
    lv_label_set_text(placeholder_, "");

    for (std::size_t index = 0; index < kGpsOnlyRowCount; ++index) {
        const auto column = index < kRowsPerColumn ? kLeftColumn : kRightColumn;
        const auto row = index < kRowsPerColumn ? index : index - kRowsPerColumn;
        const auto y = kGridTop + static_cast<std::int32_t>(row) * kRowPitch;

        auto* label = create_label(root_, Typography::body, color::text_secondary,
                                   LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(label, column, y + 4);
        lv_label_set_text(label, "");
        labels_[index] = label;
        labels_shown_[index][0] = '\0';

        auto* value = create_label(root_, Typography::heading, color::text_primary,
                                   LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_width(value, kColumnWidth);
        lv_obj_set_pos(value, column, y);
        lv_label_set_text(value, "");
        values_[index] = value;
        values_shown_[index][0] = '\0';
    }
}

void GpsOnlyScreen::update(const GpsOnlyViewModel& model) noexcept
{
    // Only write what changed. lv_label_set_text always copies and invalidates, and at
    // 120 px a needless invalidation re-rasterises a glyph - which at LVGL tick rate is
    // enough to starve the UI task, as the carousel transform and the glyph cache both
    // did before.
    const auto fixed = model.state == GpsOnlyState::fixed;

    if (std::strcmp(status_shown_.data(), model.status.data()) != 0) {
        lv_label_set_text(status_, model.status.data());
        lv_obj_set_style_text_color(status_, lv_color_hex(model.status_rgb), 0);
        std::snprintf(status_shown_.data(), status_shown_.size(), "%s",
                      model.status.data());
        // The placeholder carries the state at reading size, so it tracks the status.
        lv_label_set_text(placeholder_, fixed ? "" : model.speed.data());
        lv_obj_set_style_text_color(placeholder_, lv_color_hex(model.status_rgb), 0);
        // Whether there is a unit to show changes only when the state does.
        lv_label_set_text(unit_, fixed ? "MPH" : "");
    }

    // Right-aligned in fixed cells, so a two-digit speed leaves the leading cell blank
    // rather than padding with a zero that reads as part of the number.
    const auto length = std::strlen(model.speed.data());
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        const auto offset = digits_.size() - index;
        const char character =
            (fixed && length >= offset) ? model.speed[length - offset] : '\0';
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(digits_[index], text);
    }

    if (std::strcmp(secondary_shown_.data(), model.speed_other.data()) != 0) {
        lv_label_set_text(secondary_, model.speed_other.data());
        std::snprintf(secondary_shown_.data(), secondary_shown_.size(), "%s",
                      model.speed_other.data());
    }

    for (std::size_t index = 0; index < kGpsOnlyRowCount; ++index) {
        if (std::strcmp(labels_shown_[index].data(), model.rows[index].label.data()) != 0) {
            lv_label_set_text(labels_[index], model.rows[index].label.data());
            std::snprintf(labels_shown_[index].data(), labels_shown_[index].size(), "%s",
                          model.rows[index].label.data());
        }
        if (std::strcmp(values_shown_[index].data(), model.rows[index].value.data()) == 0) {
            continue;
        }
        lv_label_set_text(values_[index], model.rows[index].value.data());
        std::snprintf(values_shown_[index].data(), values_shown_[index].size(), "%s",
                      model.rows[index].value.data());
    }
}

}  // namespace track_timer::ui
