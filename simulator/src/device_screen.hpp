#pragma once

#include "track_timer/ui/presenter.hpp"

#include <lvgl.h>

namespace track_timer::simulator {

class DeviceScreen {
  public:
    explicit DeviceScreen(lv_obj_t* root);

    void update(const ui::DeviceViewModel& model) noexcept;

  private:
    lv_obj_t* root_;
    lv_obj_t* accent_line_;
    lv_obj_t* session_panel_;
    lv_obj_t* logging_badge_;
    lv_obj_t* lap_label_;
    lv_obj_t* gnss_label_;
    lv_obj_t* current_lap_label_;
    lv_obj_t* previous_lap_label_;
    lv_obj_t* best_lap_label_;
    lv_obj_t* logging_label_;
    lv_obj_t* session_label_;
};

}  // namespace track_timer::simulator
