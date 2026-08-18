#pragma once

#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/presenter.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

using NavigationCallback = void (*)(NavigationAction action, void* context) noexcept;

// Which controls the ready dashboard carries.
//
// On the device, Setup, Review and Diagnostics are reached by holding anywhere on the
// dashboard, so their buttons are redundant and only consume space the timer wants.
// The simulator still navigates by clicking them and has not adopted the gesture shell,
// so the full set remains the default.
enum class ReadyControls : std::uint8_t {
    all,
    start_only,
};

class ReadyScreen {
  public:
    ReadyScreen(lv_obj_t* root, NavigationCallback callback, void* callback_context,
                ReadyControls controls = ReadyControls::all) noexcept;

    void update(const ReadyViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;

    [[nodiscard]] lv_obj_t* button_for(NavigationAction action) const noexcept;
    [[nodiscard]] lv_obj_t* track_label_object() const noexcept;

    ReadyScreen(const ReadyScreen&) = delete;
    ReadyScreen& operator=(const ReadyScreen&) = delete;

  private:
    struct ButtonBinding {
        ReadyScreen* screen{nullptr};
        NavigationAction action{NavigationAction::start_session};
    };

    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* create_button(std::size_t index, NavigationAction action, const char* text,
                            std::int32_t x, std::int32_t y, std::int32_t width,
                            std::int32_t height, std::uint32_t background_rgb) noexcept;

    NavigationCallback callback_{nullptr};
    void* callback_context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* hold_hint_{nullptr};
    ReadyControls controls_{ReadyControls::all};
    lv_obj_t* track_label_{nullptr};
    lv_obj_t* session_label_{nullptr};
    lv_obj_t* rest_label_{nullptr};
    lv_obj_t* timing_mode_label_{nullptr};
    std::array<lv_obj_t*, 4> readiness_labels_{};
    std::array<lv_obj_t*, 4> buttons_{};
    std::array<ButtonBinding, 4> bindings_{};
};

}  // namespace track_timer::ui
