#pragma once

#include "track_timer/ui/gate_capture.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class GateCaptureAction : std::uint8_t {
    previous_gate,
    next_gate,
    toggle_endpoint,
    capture,
    save,
    cancel,
};

using GateCaptureCallback = void (*)(GateCaptureAction action, void* context) noexcept;

class GateCaptureScreen {
  public:
    GateCaptureScreen(lv_obj_t* root, GateCaptureCallback callback,
                      void* context) noexcept;
    void update(const GateCaptureViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(GateCaptureAction action) const noexcept;
    [[nodiscard]] lv_obj_t* status_object() const noexcept;

  private:
    struct Binding {
        GateCaptureScreen* screen{nullptr};
        GateCaptureAction action{GateCaptureAction::previous_gate};
    };
    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, GateCaptureAction action,
                          const char* text, std::int32_t x, std::int32_t y,
                          std::int32_t width, std::int32_t height,
                          std::uint32_t background_rgb) noexcept;

    GateCaptureCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* track_label_{nullptr};
    lv_obj_t* selection_label_{nullptr};
    lv_obj_t* fix_label_{nullptr};
    lv_obj_t* preview_label_{nullptr};
    lv_obj_t* status_label_{nullptr};
    std::array<lv_obj_t*, 6> buttons_{};
    std::array<Binding, 6> bindings_{};
};

}  // namespace track_timer::ui
