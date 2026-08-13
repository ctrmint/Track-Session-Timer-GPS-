#pragma once

#include "fixed_cell_label.hpp"

#include "track_timer/ui/rest_session.hpp"

#include <lvgl.h>

#include <cstdint>

namespace track_timer::simulator {

enum class RestScreenAction : std::uint8_t {
    press_skip,
    release_skip,
    cancel_skip_hold,
    cancel_skip,
    confirm_skip,
};

using RestScreenCallback = void (*)(RestScreenAction action, void* context) noexcept;

class RestScreen {
  public:
    RestScreen(lv_obj_t* root, RestScreenCallback callback, void* context) noexcept;

    void update(const ui::RestSessionViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(RestScreenAction action) const noexcept;
    [[nodiscard]] lv_obj_t* title_object() const noexcept;
    [[nodiscard]] lv_obj_t* completion_object() const noexcept;
    [[nodiscard]] lv_obj_t* remaining_object() const noexcept;

  private:
    static void skip_button_event(lv_event_t* event) noexcept;
    static void confirmation_button_event(lv_event_t* event) noexcept;
    void emit(RestScreenAction action) noexcept;

    RestScreenCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* completion_{nullptr};
    lv_obj_t* next_step_{nullptr};
    lv_obj_t* skip_button_{nullptr};
    lv_obj_t* skip_label_{nullptr};
    lv_obj_t* cancel_button_{nullptr};
    lv_obj_t* confirm_button_{nullptr};
    FixedCellLabel remaining_{};
};

}  // namespace track_timer::simulator
