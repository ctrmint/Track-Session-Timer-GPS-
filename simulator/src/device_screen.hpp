#pragma once

#include "fixed_cell_label.hpp"

#include "track_timer/ui/active_session.hpp"

#include <lvgl.h>

namespace track_timer::simulator {

enum class DeviceScreenAction : std::uint8_t {
    press_stop,
    release_stop,
    cancel_stop_hold,
    cancel_stop,
    confirm_stop,
};

using DeviceScreenCallback = void (*)(DeviceScreenAction action, void* context) noexcept;

class DeviceScreen {
  public:
    explicit DeviceScreen(lv_obj_t* root, DeviceScreenCallback callback = nullptr,
                          void* callback_context = nullptr) noexcept;

    void update(const ui::ActiveSessionViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;

    [[nodiscard]] lv_obj_t* button_for(DeviceScreenAction action) const noexcept;
    [[nodiscard]] lv_obj_t* feedback_panel_object() const noexcept;
    [[nodiscard]] lv_obj_t* feedback_comparison_object() const noexcept;
    [[nodiscard]] lv_obj_t* session_panel_object() const noexcept;

    [[nodiscard]] lv_obj_t* gnss_indicator_object() const noexcept
    {
        return gnss_label_;
    }

    [[nodiscard]] lv_obj_t* session_status_object() const noexcept
    {
        return session_caption_;
    }

  private:
    static void stop_button_event(lv_event_t* event) noexcept;
    static void confirmation_button_event(lv_event_t* event) noexcept;
    void emit(DeviceScreenAction action) noexcept;

    DeviceScreenCallback callback_{nullptr};
    void* callback_context_{nullptr};
    lv_obj_t* root_;
    lv_obj_t* accent_line_;
    lv_obj_t* session_panel_;
    lv_obj_t* logging_badge_;
    lv_obj_t* lap_label_;
    lv_obj_t* gnss_label_;
    lv_obj_t* logging_label_;
    lv_obj_t* session_caption_;
    lv_obj_t* current_caption_;
    lv_obj_t* feedback_panel_;
    lv_obj_t* feedback_heading_;
    lv_obj_t* feedback_time_;
    lv_obj_t* feedback_comparison_;
    lv_obj_t* stop_button_;
    lv_obj_t* stop_label_;
    lv_obj_t* cancel_button_;
    lv_obj_t* confirm_button_;
    FixedCellLabel current_lap_label_{};
    FixedCellLabel previous_lap_label_{};
    FixedCellLabel best_lap_label_{};
    FixedCellLabel session_label_{};
};

}  // namespace track_timer::simulator
