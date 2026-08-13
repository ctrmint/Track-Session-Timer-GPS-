#include "fixed_cell_label.hpp"

#include <algorithm>

namespace track_timer::simulator {

void FixedCellLabel::create(lv_obj_t* parent, const lv_font_t* font, const lv_color_t color,
                            const std::size_t cell_count, const FixedCellWidths& cell_widths,
                            const std::int32_t height) noexcept
{
    cell_count_ = std::min(cell_count, kMaximumFixedTextCells);
    cell_widths_ = cell_widths;
    width_ = 0;
    for (std::size_t index = 0; index < cell_count_; ++index) {
        width_ += cell_widths_[index];
    }
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, width(), height);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    std::int32_t cell_x = 0;
    for (std::size_t index = 0; index < cell_count_; ++index) {
        auto* cell = lv_label_create(root_);
        cells_[index] = cell;
        lv_obj_set_pos(cell, cell_x, 0);
        lv_obj_set_size(cell, cell_widths_[index], height);
        lv_obj_set_style_text_font(cell, font, 0);
        lv_obj_set_style_text_color(cell, color, 0);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, "");
        cell_x += cell_widths_[index];
    }
}

void FixedCellLabel::set_position(const std::int32_t x, const std::int32_t y) noexcept
{
    lv_obj_set_pos(root_, x, y);
}

void FixedCellLabel::set_text(const std::string_view text) noexcept
{
    const auto layout = layout_fixed_cell_text(text, cell_count_);
    for (std::size_t index = 0; index < cell_count_; ++index) {
        const char character = layout.cells[index];
        const char cell_text[2]{character, '\0'};
        lv_label_set_text(cells_[index], character == '\0' ? "" : cell_text);
    }
}

void FixedCellLabel::set_color(const lv_color_t color) noexcept
{
    for (std::size_t index = 0; index < cell_count_; ++index) {
        lv_obj_set_style_text_color(cells_[index], color, 0);
    }
}

std::size_t FixedCellLabel::cell_count() const noexcept
{
    return cell_count_;
}

std::int32_t FixedCellLabel::width() const noexcept
{
    return width_;
}

std::int32_t FixedCellLabel::cell_x(const std::size_t index) const noexcept
{
    return index < cell_count_ ? lv_obj_get_x(cells_[index]) : -1;
}

lv_obj_t* FixedCellLabel::object() const noexcept
{
    return root_;
}

}  // namespace track_timer::simulator
