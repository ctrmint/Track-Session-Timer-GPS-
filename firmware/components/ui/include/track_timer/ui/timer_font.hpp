#pragma once

#include <lvgl.h>

#include <cstdint>

namespace track_timer::ui {

// A real font rendered at whatever size the countdown needs.
//
// LVGL's largest built-in font is 48 px, about 3.9 mm on this 311 PPI panel. Enlarging a
// glyph with transform_scale runs LVGL's software image transform and starves the UI
// task, so the only route to a genuinely large numeral is rasterising a real face.
//
// This requires LVGL to use the C library allocator: its built-in allocator is one fixed
// 64 KB pool, and a large glyph bitmap is tens of kilobytes, so the cache thrashes.
//
// Montserrat Bold, subset to digits and separators - twelve glyphs, about 4 KB.
// Licence in fonts/OFL.txt.
[[nodiscard]] const lv_font_t* countdown_font(std::int32_t size_px) noexcept;

}  // namespace track_timer::ui
