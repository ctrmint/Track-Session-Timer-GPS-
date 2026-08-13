#pragma once

#include <lvgl.h>

#include <cstdint>

namespace track_timer::ui {

enum class Typography : std::uint8_t {
    caption,
    body,
    heading,
    timer_secondary,
    timer_primary,
};

[[nodiscard]] const lv_font_t* font_for(Typography typography) noexcept;
[[nodiscard]] lv_obj_t* create_label(lv_obj_t* parent, Typography typography,
                                     std::uint32_t text_rgb,
                                     lv_text_align_t alignment = LV_TEXT_ALIGN_CENTER) noexcept;
void style_screen(lv_obj_t* screen) noexcept;
void style_flat_panel(lv_obj_t* panel, std::uint32_t background_rgb,
                      std::int32_t radius = 0) noexcept;

}  // namespace track_timer::ui
