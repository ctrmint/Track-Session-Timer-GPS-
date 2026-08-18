#include "track_timer/ui/carousel_screen.hpp"

#include "track_timer/ui/lvgl_visual_system.hpp"

#include <algorithm>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kScreenHeight = 450;
constexpr std::int32_t kChevronWidth = 96;   // comfortably beyond the 56 px minimum
constexpr std::int32_t kDotSize = 14;
constexpr std::int32_t kDotSpacing = 30;

// LVGL's largest built-in font is 48 px, which is only about 3.9 mm on this 311 PPI
// panel. Scaling the glyph is a stopgap until the custom icon font in issue #128 exists;
// it gets the layout and interaction right now, and the icon sharpens later.
//
// 3.2x rather than 4x, so the glyph clears the name beneath it.
constexpr std::int32_t kIconScale = (32 * LV_SCALE_NONE) / 10;

constexpr std::uint32_t kBackgroundRgb = 0x000000;
constexpr std::uint32_t kMutedRgb = 0x5A6472;
constexpr std::uint32_t kAccentRgb = 0x35B0FF;

// Ruby, lightened toward rose. Red carries only 0.2126 of the WCAG luminance weight, so
// a pure red tops out at 5.25:1 on black and a deep ruby manages just 2.49:1 - below
// even the 3:1 large-text floor. Lightening the same hue is the only way to keep the
// ruby identity and still be readable at a glance: this measures 9.71:1.
constexpr std::uint32_t kLabelRubyRgb = 0xFF8FA3;

// The icon sits high so the name has room beneath it, and both stay clear of the
// position dots and the gesture hint along the bottom.
constexpr std::int32_t kIconOffsetY = -75;
constexpr std::int32_t kLabelOffsetY = 80;

}  // namespace

CarouselScreen::CarouselScreen(lv_obj_t* const root, const InputCallback callback,
                               void* const context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackgroundRgb), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    title_ = create_label(root_, Typography::body, kMutedRgb);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 18);

    icon_ = create_label(root_, Typography::timer_primary, 0xFFFFFF);
    // The pivot defaults to the object's top-left corner, so a scaled glyph grows right
    // and down instead of outward from its own centre. That is what pushed the icon off
    // centre and over the name below it.
    lv_obj_set_style_transform_pivot_x(icon_, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(icon_, lv_pct(50), 0);
    lv_obj_set_style_transform_scale_x(icon_, kIconScale, 0);
    lv_obj_set_style_transform_scale_y(icon_, kIconScale, 0);
    lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(icon_, LV_ALIGN_CENTER, 0, kIconOffsetY);

    label_ = create_label(root_, Typography::timer_secondary, kLabelRubyRgb);
    // A step up from the 28 px secondary-timer role. Sized here rather than in the
    // shared type scale because rebuilding that scale in millimetres belongs to #127.
    lv_obj_set_style_text_font(label_, &lv_font_montserrat_36, 0);
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, kLabelOffsetY);

    hint_ = create_label(root_, Typography::caption, kMutedRgb);
    lv_label_set_text(hint_, "swipe to change  -  press to select  -  swipe down to go back");
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -12);

    static constexpr std::array<const char*, 2> kChevronText{LV_SYMBOL_LEFT,
                                                             LV_SYMBOL_RIGHT};
    for (std::size_t index = 0; index < chevrons_.size(); ++index) {
        auto* button = lv_button_create(root_);
        lv_obj_set_size(button, kChevronWidth, kScreenHeight - 150);
        lv_obj_align(button, index == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        auto* glyph = create_label(button, Typography::heading, kMutedRgb);
        lv_label_set_text(glyph, kChevronText[index]);
        lv_obj_center(glyph);
        lv_obj_add_event_cb(button, chevron_event, LV_EVENT_SHORT_CLICKED, this);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<std::uintptr_t>(index)));
        chevrons_[index] = button;
    }

    for (auto*& dot : dots_) {
        dot = lv_obj_create(root_);
        lv_obj_set_size(dot, kDotSize, kDotSize);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    }
}

void CarouselScreen::chevron_event(lv_event_t* const event) noexcept
{
    auto* self = static_cast<CarouselScreen*>(lv_event_get_user_data(event));
    auto* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (self == nullptr || self->callback_ == nullptr || button == nullptr) {
        return;
    }
    const auto index =
        static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(lv_obj_get_user_data(button)));
    self->callback_(index == 0 ? InputAction::swipe_right : InputAction::swipe_left,
                    self->context_);
}

void CarouselScreen::set_entries(const CarouselEntry* const entries,
                                 const std::size_t count) noexcept
{
    count_ = std::min(count, entries_.size());
    for (std::size_t index = 0; index < count_; ++index) {
        entries_[index] = entries[index];
    }
    index_ = std::min(index_, count_ == 0 ? 0U : count_ - 1U);
    refresh();
}

void CarouselScreen::set_position(const std::size_t index) noexcept
{
    index_ = count_ == 0 ? 0 : std::min(index, count_ - 1);
    refresh();
}

void CarouselScreen::set_title(const char* const title) noexcept
{
    lv_label_set_text(title_, title == nullptr ? "" : title);
}

void CarouselScreen::refresh() noexcept
{
    if (count_ == 0) {
        lv_label_set_text(icon_, "");
        lv_label_set_text(label_, "");
        return;
    }
    const auto& entry = entries_[index_];
    lv_label_set_text(icon_, entry.icon == nullptr ? "" : entry.icon);
    lv_obj_set_style_text_color(icon_, lv_color_hex(entry.icon_rgb), 0);
    lv_label_set_text(label_, entry.label == nullptr ? "" : entry.label);

    const auto total_width =
        static_cast<std::int32_t>(count_ - 1) * kDotSpacing;
    for (std::size_t index = 0; index < dots_.size(); ++index) {
        auto* dot = dots_[index];
        if (index >= count_) {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(
            dot, lv_color_hex(index == index_ ? kAccentRgb : kMutedRgb), 0);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID,
                     static_cast<std::int32_t>(index) * kDotSpacing - total_width / 2,
                     -48);
    }
}

lv_obj_t* CarouselScreen::root() const noexcept { return root_; }

}  // namespace track_timer::ui
