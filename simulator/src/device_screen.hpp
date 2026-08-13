#pragma once

#include "fixed_cell_label.hpp"

#include "track_timer/ui/presenter.hpp"

#include <lvgl.h>

namespace track_timer::simulator {

class DeviceScreen {
  public:
    explicit DeviceScreen(lv_obj_t* root);

    void update(const ui::DeviceViewModel& model) noexcept;

    [[nodiscard]] lv_obj_t* gnss_indicator_object() const noexcept
    {
        return gnss_label_;
    }

    [[nodiscard]] lv_obj_t* session_status_object() const noexcept
    {
        return session_caption_;
    }

  private:
    lv_obj_t* root_;
    lv_obj_t* accent_line_;
    lv_obj_t* session_panel_;
    lv_obj_t* logging_badge_;
    lv_obj_t* lap_label_;
    lv_obj_t* gnss_label_;
    lv_obj_t* logging_label_;
    lv_obj_t* session_caption_;
    FixedCellLabel current_lap_label_{};
    FixedCellLabel previous_lap_label_{};
    FixedCellLabel best_lap_label_{};
    FixedCellLabel session_label_{};
};

}  // namespace track_timer::simulator
