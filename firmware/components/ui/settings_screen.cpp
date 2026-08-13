#include "track_timer/ui/settings_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {

SettingsScreen::SettingsScreen(lv_obj_t* root, const SettingsScreenCallback callback,
                               void* callback_context) noexcept
    : callback_(callback), callback_context_(callback_context)
{
    root_ = root;
    style_screen(root);

    auto* title = create_label(root, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " SETTINGS");
    lv_obj_set_pos(title, 20, 14);
    lv_obj_set_size(title, 300, 36);

    auto* field_panel = lv_obj_create(root);
    style_flat_panel(field_panel, color::surface, 12);
    lv_obj_set_pos(field_panel, 20, 62);
    lv_obj_set_size(field_panel, 560, 105);

    field_label_ = create_label(field_panel, Typography::caption, color::text_secondary);
    lv_obj_set_pos(field_label_, 10, 10);
    lv_obj_set_size(field_label_, 540, 24);

    value_label_ = create_label(field_panel, Typography::heading, color::text_primary);
    lv_obj_set_pos(value_label_, 10, 47);
    lv_obj_set_size(value_label_, 540, 36);

    buttons_[0] = make_button(0, SettingsScreenAction::previous_field, LV_SYMBOL_LEFT " FIELD",
                              20, 182, 130, 62, color::surface);
    buttons_[1] = make_button(1, SettingsScreenAction::decrement, LV_SYMBOL_MINUS " VALUE", 165,
                              182, 125, 62, color::surface);
    buttons_[2] = make_button(2, SettingsScreenAction::increment, LV_SYMBOL_PLUS " VALUE", 305,
                              182, 125, 62, color::surface);
    buttons_[3] = make_button(3, SettingsScreenAction::next_field, "FIELD " LV_SYMBOL_RIGHT, 445,
                              182, 135, 62, color::surface);

    status_label_ = create_label(root, Typography::caption, color::caution_bright);
    lv_obj_set_pos(status_label_, 20, 259);
    lv_obj_set_size(status_label_, 560, 38);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);

    buttons_[4] = make_button(4, SettingsScreenAction::save, LV_SYMBOL_OK " SAVE", 20, 315, 175,
                              110, color::positive);
    buttons_[5] = make_button(5, SettingsScreenAction::cancel, LV_SYMBOL_CLOSE " CANCEL", 210,
                              315, 175, 110, color::surface);
    buttons_[6] = make_button(6, SettingsScreenAction::restore_defaults, LV_SYMBOL_REFRESH " DEFAULTS",
                              400, 315, 180, 110, color::warning);
}

void SettingsScreen::update(const SettingsEditorViewModel& model) noexcept
{
    confirming_defaults_ = model.confirming_defaults;
    lv_label_set_text(field_label_, model.field_name.data());
    lv_label_set_text(value_label_, model.value.data());
    lv_label_set_text(status_label_, model.status.data());
    lv_obj_set_style_text_color(status_label_,
                                lv_color_hex(model.editing ? color::caution_bright
                                                          : color::text_secondary),
                                0);

    auto* save_label = lv_obj_get_child(buttons_[4], 0);
    auto* cancel_label = lv_obj_get_child(buttons_[5], 0);
    lv_label_set_text(save_label,
                      confirming_defaults_ ? LV_SYMBOL_WARNING " CONFIRM" : LV_SYMBOL_OK " SAVE");
    lv_label_set_text(cancel_label, confirming_defaults_ ? LV_SYMBOL_CLOSE " KEEP"
                                                         : LV_SYMBOL_CLOSE " CANCEL");

    for (std::size_t index = 0; index < 4; ++index) {
        if (model.editing && !confirming_defaults_) {
            lv_obj_remove_state(buttons_[index], LV_STATE_DISABLED);
        }
        else {
            lv_obj_add_state(buttons_[index], LV_STATE_DISABLED);
        }
    }
    if (model.editing || confirming_defaults_) {
        lv_obj_remove_state(buttons_[4], LV_STATE_DISABLED);
        lv_obj_remove_state(buttons_[5], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[4], LV_STATE_DISABLED);
        lv_obj_remove_state(buttons_[5], LV_STATE_DISABLED);
    }
    if (model.editing && !confirming_defaults_) {
        lv_obj_remove_state(buttons_[6], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[6], LV_STATE_DISABLED);
    }
}

void SettingsScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* SettingsScreen::button_for(const SettingsScreenAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    if (confirming_defaults_ && action == SettingsScreenAction::confirm_defaults) {
        return buttons_[4];
    }
    if (confirming_defaults_ && action == SettingsScreenAction::cancel_defaults) {
        return buttons_[5];
    }
    return nullptr;
}

lv_obj_t* SettingsScreen::field_label_object() const noexcept
{
    return field_label_;
}

lv_obj_t* SettingsScreen::value_label_object() const noexcept
{
    return value_label_;
}

lv_obj_t* SettingsScreen::status_label_object() const noexcept
{
    return status_label_;
}

void SettingsScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<ButtonBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->screen == nullptr ||
        binding->screen->callback_ == nullptr) {
        return;
    }
    auto action = binding->action;
    if (binding->screen->confirming_defaults_) {
        if (action == SettingsScreenAction::save) {
            action = SettingsScreenAction::confirm_defaults;
        }
        else if (action == SettingsScreenAction::cancel) {
            action = SettingsScreenAction::cancel_defaults;
        }
    }
    binding->screen->callback_(action, binding->screen->callback_context_);
}

lv_obj_t* SettingsScreen::make_button(const std::size_t index,
                                      const SettingsScreenAction action, const char* text,
                                      const std::int32_t x, const std::int32_t y,
                                      const std::int32_t width, const std::int32_t height,
                                      const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 12);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    auto* label = create_label(button, Typography::body, contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    bindings_[index] = ButtonBinding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui
