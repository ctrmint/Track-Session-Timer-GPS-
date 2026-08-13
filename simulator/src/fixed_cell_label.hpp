#pragma once

#include "track_timer/simulator/fixed_cell_text.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

using FixedCellWidths = std::array<std::int32_t, kMaximumFixedTextCells>;

[[nodiscard]] constexpr FixedCellWidths lap_time_cell_widths(const std::int32_t digit_width,
                                                             const std::int32_t colon_width,
                                                             const std::int32_t decimal_width) noexcept
{
    return FixedCellWidths{digit_width, digit_width, digit_width, colon_width, digit_width,
                           digit_width, decimal_width, digit_width, digit_width, digit_width};
}

[[nodiscard]] constexpr FixedCellWidths session_time_cell_widths(
    const std::int32_t digit_width, const std::int32_t colon_width) noexcept
{
    return FixedCellWidths{digit_width, digit_width, digit_width, digit_width, colon_width,
                           digit_width, digit_width, 0, 0, 0};
}

class FixedCellLabel {
  public:
    FixedCellLabel() = default;

    void create(lv_obj_t* parent, const lv_font_t* font, lv_color_t color,
                std::size_t cell_count, const FixedCellWidths& cell_widths,
                std::int32_t height) noexcept;
    void set_position(std::int32_t x, std::int32_t y) noexcept;
    void set_text(std::string_view text) noexcept;
    void set_color(lv_color_t color) noexcept;

    [[nodiscard]] std::size_t cell_count() const noexcept;
    [[nodiscard]] std::int32_t width() const noexcept;
    [[nodiscard]] std::int32_t cell_x(std::size_t index) const noexcept;
    [[nodiscard]] lv_obj_t* object() const noexcept;

    FixedCellLabel(const FixedCellLabel&) = delete;
    FixedCellLabel& operator=(const FixedCellLabel&) = delete;

  private:
    lv_obj_t* root_{nullptr};
    std::array<lv_obj_t*, kMaximumFixedTextCells> cells_{};
    FixedCellWidths cell_widths_{};
    std::size_t cell_count_{0};
    std::int32_t width_{0};
};

}  // namespace track_timer::simulator
