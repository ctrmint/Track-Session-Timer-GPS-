#include "track_timer/ui/track_selection_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {

TrackSelectionScreen::TrackSelectionScreen(lv_obj_t* root,
                                           const TrackSelectionCallback callback,
                                           void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, LV_SYMBOL_GPS " SELECT TRACK");
    lv_obj_set_pos(title, 20, 14);
    lv_obj_set_size(title, 300, 36);

    auto* panel = lv_obj_create(root_);
    style_flat_panel(panel, color::surface, 12);
    lv_obj_set_pos(panel, 20, 60);
    lv_obj_set_size(panel, 560, 142);
    position_label_ = create_label(panel, Typography::caption, color::text_secondary);
    lv_obj_set_pos(position_label_, 12, 10);
    lv_obj_set_size(position_label_, 536, 22);
    track_name_label_ = create_label(panel, Typography::heading, color::text_primary);
    lv_obj_set_pos(track_name_label_, 12, 48);
    lv_obj_set_size(track_name_label_, 536, 34);
    definition_label_ = create_label(panel, Typography::caption, color::positive_bright);
    lv_obj_set_pos(definition_label_, 12, 104);
    lv_obj_set_size(definition_label_, 536, 24);

    buttons_[0] = make_button(0, TrackSelectionAction::previous, LV_SYMBOL_LEFT " TRACK", 20,
                              216, 150, 62, color::surface);
    buttons_[1] = make_button(1, TrackSelectionAction::select, LV_SYMBOL_OK " SELECT TRACK", 185,
                              216, 230, 62, color::positive);
    buttons_[2] = make_button(2, TrackSelectionAction::next, "TRACK " LV_SYMBOL_RIGHT, 430,
                              216, 150, 62, color::surface);

    status_label_ = create_label(root_, Typography::caption, color::caution_bright);
    lv_obj_set_pos(status_label_, 20, 290);
    lv_obj_set_size(status_label_, 560, 40);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_WRAP);

    buttons_[3] = make_button(3, TrackSelectionAction::timer_only, LV_SYMBOL_PLAY " TIMER ONLY",
                              20, 350, 175, 80, color::caution);
    buttons_[4] = make_button(4, TrackSelectionAction::capture_information,
                              LV_SYMBOL_PLUS " CAPTURE INFO", 210, 350, 195, 80,
                              color::surface);
    buttons_[5] = make_button(5, TrackSelectionAction::back, LV_SYMBOL_LEFT " SETUP", 420, 350,
                              160, 80, color::surface);
}

void TrackSelectionScreen::update(const TrackSelectionViewModel& model) noexcept
{
    lv_label_set_text(position_label_, model.position.data());
    lv_label_set_text(track_name_label_, model.track_name.data());
    lv_label_set_text(definition_label_, model.definition.data());
    lv_label_set_text(status_label_, model.status.data());
    lv_obj_set_style_text_color(status_label_, lv_color_hex(model.status_color_rgb), 0);
    lv_label_set_text(lv_obj_get_child(buttons_[1], 0), model.select_label.data());
    for (const auto index : {0U, 2U}) {
        if (model.can_browse) {
            lv_obj_remove_state(buttons_[index], LV_STATE_DISABLED);
        }
        else {
            lv_obj_add_state(buttons_[index], LV_STATE_DISABLED);
        }
    }
    if (model.can_select) {
        lv_obj_remove_state(buttons_[1], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[1], LV_STATE_DISABLED);
    }
    if (model.can_use_timer_only) {
        lv_obj_remove_state(buttons_[3], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[3], LV_STATE_DISABLED);
    }
}

void TrackSelectionScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* TrackSelectionScreen::button_for(const TrackSelectionAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

lv_obj_t* TrackSelectionScreen::track_name_object() const noexcept
{
    return track_name_label_;
}

lv_obj_t* TrackSelectionScreen::status_object() const noexcept
{
    return status_label_;
}

void TrackSelectionScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

lv_obj_t* TrackSelectionScreen::make_button(const std::size_t index,
                                            const TrackSelectionAction action,
                                            const char* text, const std::int32_t x,
                                            const std::int32_t y, const std::int32_t width,
                                            const std::int32_t height,
                                            const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 12);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    auto* label = create_label(button, Typography::body, contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    bindings_[index] = Binding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui
