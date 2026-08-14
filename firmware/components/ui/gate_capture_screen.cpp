#include "track_timer/ui/gate_capture_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {

GateCaptureScreen::GateCaptureScreen(lv_obj_t* root,
                                     const GateCaptureCallback callback,
                                     void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, LV_SYMBOL_GPS " CAPTURE CIRCUIT GATES");
    lv_obj_set_pos(title, 20, 12);
    lv_obj_set_size(title, 500, 34);

    auto* panel = lv_obj_create(root_);
    style_flat_panel(panel, color::surface, 12);
    lv_obj_set_pos(panel, 20, 54);
    lv_obj_set_size(panel, 560, 184);
    track_label_ = create_label(panel, Typography::body, color::text_primary);
    lv_obj_set_pos(track_label_, 12, 8);
    lv_obj_set_size(track_label_, 536, 28);
    selection_label_ = create_label(panel, Typography::heading, color::positive_bright);
    lv_obj_set_pos(selection_label_, 12, 42);
    lv_obj_set_size(selection_label_, 536, 34);
    fix_label_ = create_label(panel, Typography::caption, color::text_secondary);
    lv_obj_set_pos(fix_label_, 12, 88);
    lv_obj_set_size(fix_label_, 536, 26);
    preview_label_ = create_label(panel, Typography::caption, color::text_secondary);
    lv_obj_set_pos(preview_label_, 12, 126);
    lv_obj_set_size(preview_label_, 536, 42);

    buttons_[0] = make_button(0, GateCaptureAction::previous_gate, LV_SYMBOL_LEFT " GATE",
                              20, 250, 120, 54, color::surface);
    buttons_[1] = make_button(1, GateCaptureAction::toggle_endpoint, "LEFT ENDPOINT",
                              150, 250, 190, 54, color::surface);
    buttons_[2] = make_button(2, GateCaptureAction::next_gate, "GATE " LV_SYMBOL_RIGHT,
                              350, 250, 120, 54, color::surface);
    buttons_[3] = make_button(3, GateCaptureAction::capture, LV_SYMBOL_PLUS " CAPTURE",
                              480, 250, 100, 54, color::positive);

    status_label_ = create_label(root_, Typography::caption, color::caution_bright);
    lv_obj_set_pos(status_label_, 20, 316);
    lv_obj_set_size(status_label_, 560, 42);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);

    buttons_[4] = make_button(4, GateCaptureAction::save, LV_SYMBOL_SAVE " SAVE JSON",
                              20, 370, 360, 60, color::positive);
    buttons_[5] = make_button(5, GateCaptureAction::cancel, LV_SYMBOL_CLOSE " CANCEL",
                              400, 370, 180, 60, color::surface);
}

void GateCaptureScreen::update(const GateCaptureViewModel& model) noexcept
{
    lv_label_set_text(track_label_, model.track.data());
    lv_label_set_text(selection_label_, model.selection.data());
    lv_label_set_text(fix_label_, model.fix.data());
    lv_label_set_text(preview_label_, model.preview.data());
    lv_label_set_text(status_label_, model.status.data());
    lv_obj_set_style_text_color(status_label_, lv_color_hex(model.status_color_rgb), 0);
    lv_label_set_text(lv_obj_get_child(buttons_[1], 0), model.endpoint_label.data());
    lv_label_set_text(lv_obj_get_child(buttons_[4], 0), model.save_label.data());
    if (model.can_capture) {
        lv_obj_remove_state(buttons_[3], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[3], LV_STATE_DISABLED);
    }
    if (model.can_save || model.confirming_overwrite) {
        lv_obj_remove_state(buttons_[4], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[4], LV_STATE_DISABLED);
    }
}

void GateCaptureScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* GateCaptureScreen::button_for(const GateCaptureAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

lv_obj_t* GateCaptureScreen::status_object() const noexcept
{
    return status_label_;
}

void GateCaptureScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

lv_obj_t* GateCaptureScreen::make_button(
    const std::size_t index, const GateCaptureAction action, const char* text,
    const std::int32_t x, const std::int32_t y, const std::int32_t width,
    const std::int32_t height, const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 10);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    auto* label = create_label(button, Typography::body,
                               contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    bindings_[index] = Binding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui
