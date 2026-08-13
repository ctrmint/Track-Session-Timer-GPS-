#pragma once

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class SetupMenuAction : std::uint8_t {
    device_settings,
    track_selection,
    g_meter,
    back,
};

using SetupMenuCallback = void (*)(SetupMenuAction action, void* context) noexcept;

class SetupMenuScreen {
  public:
    SetupMenuScreen(lv_obj_t* root, SetupMenuCallback callback, void* context) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(SetupMenuAction action) const noexcept;

  private:
    struct Binding {
        SetupMenuScreen* screen{nullptr};
        SetupMenuAction action{SetupMenuAction::device_settings};
    };
    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, SetupMenuAction action, const char* title,
                          const char* detail, std::int32_t y,
                          std::uint32_t background_rgb) noexcept;

    SetupMenuCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    std::array<lv_obj_t*, 4> buttons_{};
    std::array<Binding, 4> bindings_{};
};

}  // namespace track_timer::ui
