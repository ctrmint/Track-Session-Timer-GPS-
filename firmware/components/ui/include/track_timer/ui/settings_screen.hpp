#pragma once

#include "track_timer/ui/settings_editor.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class SettingsScreenAction : std::uint8_t {
    previous_field,
    next_field,
    decrement,
    increment,
    save,
    cancel,
    restore_defaults,
    confirm_defaults,
    cancel_defaults,
};

using SettingsScreenCallback = void (*)(SettingsScreenAction action,
                                        void* context) noexcept;

class SettingsScreen {
  public:
    SettingsScreen(lv_obj_t* root, SettingsScreenCallback callback,
                   void* callback_context) noexcept;

    void update(const SettingsEditorViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;

    [[nodiscard]] lv_obj_t* button_for(SettingsScreenAction action) const noexcept;
    [[nodiscard]] lv_obj_t* field_label_object() const noexcept;
    [[nodiscard]] lv_obj_t* value_label_object() const noexcept;
    [[nodiscard]] lv_obj_t* status_label_object() const noexcept;

    SettingsScreen(const SettingsScreen&) = delete;
    SettingsScreen& operator=(const SettingsScreen&) = delete;

  private:
    struct ButtonBinding {
        SettingsScreen* screen{nullptr};
        SettingsScreenAction action{SettingsScreenAction::previous_field};
    };

    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, SettingsScreenAction action, const char* text,
                          std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t height, std::uint32_t background_rgb) noexcept;

    SettingsScreenCallback callback_{nullptr};
    void* callback_context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* field_label_{nullptr};
    lv_obj_t* value_label_{nullptr};
    lv_obj_t* status_label_{nullptr};
    std::array<lv_obj_t*, 7> buttons_{};
    std::array<ButtonBinding, 7> bindings_{};
    bool confirming_defaults_{false};
};

}  // namespace track_timer::ui
