#include "track_timer/ui/setup_menu_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {

SetupMenuScreen::SetupMenuScreen(lv_obj_t* root, const SetupMenuCallback callback,
                                 void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " SETUP");
    lv_obj_set_pos(title, 20, 18);
    lv_obj_set_size(title, 300, 38);

    buttons_[0] = make_button(0, SetupMenuAction::track_selection,
                              LV_SYMBOL_GPS " TRACK SELECTION",
                              "CHOOSE TRACK AND CHECK TIMING READINESS", 64, color::positive);
    buttons_[1] = make_button(1, SetupMenuAction::device_settings,
                              LV_SYMBOL_SETTINGS " DEVICE SETTINGS",
                              "SESSION, DISPLAY, LAUNCH AND MODE", 157, color::surface);
    buttons_[2] = make_button(2, SetupMenuAction::g_meter, "G-METER / IMU",
                              "LIVE ACCELERATION, PEAKS AND SENSOR HEALTH", 250,
                              color::surface);
    buttons_[3] = make_button(3, SetupMenuAction::back, LV_SYMBOL_LEFT " BACK TO READY",
                              "DISCARD NO SAVED CHANGES", 343, color::surface);
}

void SetupMenuScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* SetupMenuScreen::button_for(const SetupMenuAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

void SetupMenuScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

lv_obj_t* SetupMenuScreen::make_button(const std::size_t index,
                                       const SetupMenuAction action, const char* title,
                                       const char* detail, const std::int32_t y,
                                       const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 12);
    lv_obj_set_pos(button, 20, y);
    lv_obj_set_size(button, 560, 81);
    auto* title_label = create_label(button, Typography::body,
                                     contrast_text_rgb(background_rgb), LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title_label, title);
    lv_obj_set_pos(title_label, 20, 10);
    lv_obj_set_size(title_label, 520, 30);
    auto* detail_label = create_label(button, Typography::caption,
                                      contrast_text_rgb(background_rgb), LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(detail_label, detail);
    lv_obj_set_pos(detail_label, 20, 47);
    lv_obj_set_size(detail_label, 520, 24);
    bindings_[index] = Binding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui
