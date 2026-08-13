#include "track_timer/ui/lvgl_visual_system.hpp"

#include "track_timer/ui/foundation.hpp"

namespace track_timer::ui {

const lv_font_t* font_for(const Typography typography) noexcept
{
    switch (typography) {
    case Typography::caption:
        return &lv_font_montserrat_14;
    case Typography::body:
        return &lv_font_montserrat_20;
    case Typography::heading:
        return &lv_font_montserrat_24;
    case Typography::timer_secondary:
        return &lv_font_montserrat_28;
    case Typography::timer_primary:
        return &lv_font_montserrat_48;
    }
    return &lv_font_montserrat_14;
}

lv_obj_t* create_label(lv_obj_t* parent, const Typography typography,
                       const std::uint32_t text_rgb, const lv_text_align_t alignment) noexcept
{
    auto* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font_for(typography), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(text_rgb), 0);
    lv_obj_set_style_text_align(label, alignment, 0);
    return label;
}

void style_screen(lv_obj_t* screen) noexcept
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(color::background), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

void style_flat_panel(lv_obj_t* panel, const std::uint32_t background_rgb,
                      const std::int32_t radius) noexcept
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(background_rgb), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, radius, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace track_timer::ui
