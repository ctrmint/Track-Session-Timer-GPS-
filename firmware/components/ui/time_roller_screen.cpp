#include "track_timer/ui/time_roller_screen.hpp"

#include "track_timer/ui/lvgl_visual_system.hpp"

#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kSideMargin = 24;
constexpr std::int32_t kAvailableWidth = kScreenWidth - 2 * kSideMargin;
constexpr std::int32_t kDigitsTop = 118;

// The same face and metrics the countdown uses: the widest numeral advances 689/1000 em,
// so a cell is that plus a little side bearing, and the colon is much narrower.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t colon_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 288 / 1000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

// Digits needed for the largest minute value the field allows: 59 needs two, 1439 needs
// four, and a duration column has to be built for its own ceiling rather than a shared one.
std::size_t minute_digits_for(const std::uint16_t maximum_minutes) noexcept
{
    std::size_t digits = 1;
    for (auto value = maximum_minutes; value >= 10; value /= 10) {
        ++digits;
    }
    return digits < 2 ? 2 : digits;
}

// The largest size that fits the width, capped so the wheel still has room above and below
// on a 450 px panel. Average lap gets 140 px, about 8 mm; the durations need four minute
// digits so they settle lower.
std::int32_t font_px_for(const std::size_t minute_digits) noexcept
{
    for (const std::int32_t candidate : {140, 130, 120, 115, 100, 90}) {
        const auto total =
            cell_width_for(candidate) * (static_cast<std::int32_t>(minute_digits) + 2) +
            colon_width_for(candidate);
        if (total <= kAvailableWidth) {
            return candidate;
        }
    }
    return 90;
}

constexpr std::uint32_t kBackground = 0x000000;
constexpr std::uint32_t kActive = 0xF2F5F8;
constexpr std::uint32_t kInactive = 0x5C6675;
constexpr std::uint32_t kMuted = 0x7C8899;
constexpr std::uint32_t kAccent = 0x39B6FF;

}  // namespace

TimeRollerScreen::TimeRollerScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    title_ = create_label(root_, Typography::heading, kAccent);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 18);

    minutes_unit_ = create_label(root_, Typography::caption, kMuted);
    seconds_unit_ = create_label(root_, Typography::caption, kMuted);
    lv_label_set_text(minutes_unit_, "MIN");
    lv_label_set_text(seconds_unit_, "SEC");

    // Fills as a save is held. Sits directly under the hint it is confirming.
    hold_bar_ = lv_obj_create(root_);
    lv_obj_remove_style_all(hold_bar_);
    lv_obj_set_size(hold_bar_, 0, 6);
    lv_obj_set_style_bg_color(hold_bar_, lv_color_hex(kAccent), 0);
    lv_obj_set_style_bg_opa(hold_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hold_bar_, 3, 0);
    lv_obj_align(hold_bar_, LV_ALIGN_BOTTOM_MID, 0, -6);
    bubble_gestures_to_parent(hold_bar_);

    hint_ = create_label(root_, Typography::caption, kMuted);
    lv_label_set_text(hint_, "HOLD TO SAVE      SWIPE LEFT OR RIGHT TO CANCEL");
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void TimeRollerScreen::build_cells(const std::int32_t font_px,
                                   const std::size_t minute_digits) noexcept
{
    if (font_px == font_px_ && minute_digits == minute_digits_) {
        return;
    }
    for (auto*& cell : minute_cells_) {
        if (cell != nullptr) {
            lv_obj_delete(cell);
            cell = nullptr;
        }
    }
    for (auto*& cell : second_cells_) {
        if (cell != nullptr) {
            lv_obj_delete(cell);
            cell = nullptr;
        }
    }
    if (colon_ != nullptr) {
        lv_obj_delete(colon_);
        colon_ = nullptr;
    }

    font_px_ = font_px;
    minute_digits_ = minute_digits;

    const auto* font = countdown_font(font_px);
    const auto cell = cell_width_for(font_px);
    const auto colon = colon_width_for(font_px);
    const auto height = cell_height_for(font_px);
    const auto total =
        cell * (static_cast<std::int32_t>(minute_digits) + 2) + colon;
    auto x = (kScreenWidth - total) / 2;

    const auto make_cell = [&](const std::int32_t width, const char* text) {
        auto* label = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(label, font, 0);
        }
        lv_obj_set_size(label, width, height);
        lv_obj_set_pos(label, x, kDigitsTop);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(label, text);
        // A clickable child would swallow gestures that begin on it, and every touch here
        // begins on a digit.
        bubble_gestures_to_parent(label);
        x += width;
        return label;
    };

    for (std::size_t index = 0; index < minute_digits; ++index) {
        minute_cells_[index] = make_cell(cell, " ");
    }
    const auto minutes_right = x;
    colon_ = make_cell(colon, ":");
    const auto seconds_left = x;
    for (auto& slot : second_cells_) {
        slot = make_cell(cell, "0");
    }

    // The split sits in the colon, so a touch anywhere over a column's digits picks it.
    split_x_ = (minutes_right + seconds_left) / 2;

    lv_obj_align(minutes_unit_, LV_ALIGN_TOP_LEFT,
                 (kScreenWidth - total) / 2 +
                     cell * static_cast<std::int32_t>(minute_digits) / 2 - 20,
                 kDigitsTop + height + 10);
    lv_obj_align(seconds_unit_, LV_ALIGN_TOP_LEFT, seconds_left + cell - 20,
                 kDigitsTop + height + 10);
}

void TimeRollerScreen::configure(const char* const title, const SettingsField field,
                                 const std::uint32_t seconds) noexcept
{
    field_ = field;
    const auto spec = time_field_spec(field);
    const auto digits = minute_digits_for(spec.maximum_minutes);
    build_cells(font_px_for(digits), digits);

    lv_label_set_text(title_, title == nullptr ? "" : title);
    roller_.reset(seconds, spec);
    refresh();
}

void TimeRollerScreen::write_column(const bool minutes) noexcept
{
    const auto active = (roller_.active() == RollerColumn::minutes) == minutes;
    const auto colour = lv_color_hex(active ? kActive : kInactive);

    if (minutes) {
        // Right-aligned with blank leading cells: the field never changes width, but a
        // twenty minute session still reads as "20" rather than "0020".
        auto value = static_cast<std::uint32_t>(roller_.minutes());
        for (std::size_t index = minute_digits_; index-- > 0;) {
            auto* label = minute_cells_[index];
            if (label == nullptr) {
                continue;
            }
            char text[2] = {' ', '\0'};
            const auto rendered = index == minute_digits_ - 1 || value > 0;
            if (rendered) {
                text[0] = static_cast<char>('0' + (value % 10U));
            }
            value /= 10U;
            const auto* current = lv_label_get_text(label);
            if (current == nullptr || current[0] != text[0]) {
                lv_label_set_text(label, text);
            }
            lv_obj_set_style_text_color(label, colour, 0);
        }
        return;
    }

    const auto value = static_cast<std::uint32_t>(roller_.seconds());
    const char tens = static_cast<char>('0' + (value / 10U));
    const char units = static_cast<char>('0' + (value % 10U));
    const char digits[2] = {tens, units};
    for (std::size_t index = 0; index < second_cells_.size(); ++index) {
        auto* label = second_cells_[index];
        if (label == nullptr) {
            continue;
        }
        const char text[2] = {digits[index], '\0'};
        const auto* current = lv_label_get_text(label);
        if (current == nullptr || current[0] != text[0]) {
            lv_label_set_text(label, text);
        }
        lv_obj_set_style_text_color(label, colour, 0);
    }
}

void TimeRollerScreen::refresh() noexcept
{
    if (root_ == nullptr) {
        return;
    }
    write_column(true);
    write_column(false);
    if (colon_ != nullptr) {
        lv_obj_set_style_text_color(colon_, lv_color_hex(kInactive), 0);
    }
}

void TimeRollerScreen::set_hold_progress(const float fraction) noexcept
{
    if (hold_bar_ == nullptr) {
        return;
    }
    const auto clamped = fraction < 0.0F ? 0.0F : (fraction > 1.0F ? 1.0F : fraction);
    const auto width = static_cast<std::int32_t>(clamped * static_cast<float>(kAvailableWidth));
    if (width != lv_obj_get_width(hold_bar_)) {
        lv_obj_set_width(hold_bar_, width);
        lv_obj_align(hold_bar_, LV_ALIGN_BOTTOM_MID, 0, -6);
    }
}

TimeRoller& TimeRollerScreen::roller() noexcept { return roller_; }

RollerColumn TimeRollerScreen::column_at(const std::int16_t x) const noexcept
{
    return x < split_x_ ? RollerColumn::minutes : RollerColumn::seconds;
}

lv_obj_t* TimeRollerScreen::root() const noexcept { return root_; }

}  // namespace track_timer::ui
