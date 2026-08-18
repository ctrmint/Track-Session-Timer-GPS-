#include "track_timer/ui/ready_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {
namespace {

lv_obj_t* make_value_panel(lv_obj_t* parent, const std::int32_t x, const std::int32_t y,
                           const std::int32_t width) noexcept
{
    auto* panel = lv_obj_create(parent);
    style_flat_panel(panel, color::surface, 10);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, 64);
    return panel;
}

}  // namespace

ReadyScreen::ReadyScreen(lv_obj_t* root, const NavigationCallback callback,
                         void* callback_context, const ReadyControls controls) noexcept
    : callback_(callback), callback_context_(callback_context), root_(root),
      controls_(controls)
{
    style_screen(root_);

    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, "READY");
    lv_obj_set_pos(title, 20, 14);
    lv_obj_set_size(title, 120, 34);

    auto* track_caption = create_label(root_, Typography::caption, color::text_secondary,
                                       LV_TEXT_ALIGN_RIGHT);
    lv_label_set_text(track_caption, "TRACK");
    lv_obj_set_pos(track_caption, 150, 18);
    lv_obj_set_size(track_caption, 80, 24);

    track_label_ = create_label(root_, Typography::body, color::text_primary,
                                LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(track_label_, 235, 16);
    lv_obj_set_size(track_label_, 345, 30);

    auto* session_panel = make_value_panel(root_, 20, 62, 270);
    session_label_ = create_label(session_panel, Typography::body, color::text_primary);
    lv_obj_center(session_label_);

    auto* rest_panel = make_value_panel(root_, 310, 62, 270);
    rest_label_ = create_label(rest_panel, Typography::body, color::text_primary);
    lv_obj_center(rest_label_);

    constexpr std::array<std::int32_t, 4> status_x{20, 165, 310, 455};
    for (std::size_t index = 0; index < readiness_labels_.size(); ++index) {
        readiness_labels_[index] =
            create_label(root_, Typography::caption, color::critical_bright);
        lv_obj_set_pos(readiness_labels_[index], status_x[index], 146);
        lv_obj_set_size(readiness_labels_[index], 125, 42);
        lv_label_set_long_mode(readiness_labels_[index], LV_LABEL_LONG_WRAP);
    }

    timing_mode_label_ = create_label(root_, Typography::body, color::caution_bright);
    lv_obj_set_pos(timing_mode_label_, 20, 205);
    lv_obj_set_size(timing_mode_label_, 560, 34);

    if (controls_ == ReadyControls::start_only) {
        // Start takes the space the three secondary buttons occupied. The rest of the
        // dashboard is left clear so a hold has somewhere to land: a press on a button
        // is consumed by that button and never reaches the screen's gesture handler.
        buttons_[0] = create_button(0, NavigationAction::start_session,
                                    LV_SYMBOL_PLAY " START", 20, 265, 560, 120,
                                    color::positive);
        hold_hint_ = create_label(root_, Typography::caption, color::text_secondary);
        lv_label_set_text(hold_hint_, "hold anywhere for Mode, Setup, Review, Diagnostics");
        lv_obj_set_pos(hold_hint_, 20, 398);
        lv_obj_set_size(hold_hint_, 560, 30);
        return;
    }

    buttons_[0] = create_button(0, NavigationAction::start_session, LV_SYMBOL_PLAY " START",
                                20, 265, 200, 165, color::positive);
    buttons_[1] = create_button(1, NavigationAction::open_setup, LV_SYMBOL_SETTINGS " SETUP",
                                235, 265, 160, 72, color::surface);
    buttons_[2] = create_button(2, NavigationAction::open_review, LV_SYMBOL_LIST " REVIEW", 410,
                                265, 170, 72, color::surface);
    buttons_[3] = create_button(3, NavigationAction::open_diagnostics,
                                LV_SYMBOL_WARNING " DIAGNOSTICS", 235, 352, 345, 78,
                                color::surface);
}

void ReadyScreen::update(const ReadyViewModel& model) noexcept
{
    lv_label_set_text(track_label_, model.selected_track.data());
    lv_label_set_text(session_label_, model.session_duration.data());
    lv_label_set_text(rest_label_, model.rest_duration.data());
    lv_label_set_text(timing_mode_label_, model.timing_mode.data());

    const std::array<const ReadinessItem*, 4> items{
        &model.gnss,
        &model.storage,
        &model.imu,
        &model.logging,
    };
    for (std::size_t index = 0; index < readiness_labels_.size(); ++index) {
        lv_label_set_text(readiness_labels_[index], items[index]->text.data());
        lv_obj_set_style_text_color(readiness_labels_[index],
                                    lv_color_hex(items[index]->color_rgb), 0);
    }

    if (buttons_[1] != nullptr) {
        if (model.setup_enabled) {
            lv_obj_remove_state(buttons_[1], LV_STATE_DISABLED);
        }
        else {
            lv_obj_add_state(buttons_[1], LV_STATE_DISABLED);
        }
    }
}

void ReadyScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        if (button != nullptr) {
            lv_group_add_obj(group, button);
        }
    }
}

lv_obj_t* ReadyScreen::button_for(const NavigationAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

lv_obj_t* ReadyScreen::track_label_object() const noexcept
{
    return track_label_;
}

void ReadyScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<ButtonBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->screen == nullptr ||
        binding->screen->callback_ == nullptr) {
        return;
    }
    binding->screen->callback_(binding->action, binding->screen->callback_context_);
}

lv_obj_t* ReadyScreen::create_button(const std::size_t index, const NavigationAction action,
                                     const char* text, const std::int32_t x,
                                     const std::int32_t y, const std::int32_t width,
                                     const std::int32_t height,
                                     const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 12);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_add_flag(button, LV_OBJ_FLAG_EVENT_BUBBLE);

    auto* label = create_label(button, Typography::body, contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);

    bindings_[index] = ButtonBinding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui
