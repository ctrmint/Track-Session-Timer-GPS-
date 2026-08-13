#include "application_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::simulator {
namespace {

lv_obj_t* make_screen_root(lv_obj_t* parent) noexcept
{
    auto* root = lv_obj_create(parent);
    ui::style_screen(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, 600, 450);
    return root;
}

}  // namespace

ApplicationScreen::ApplicationScreen(lv_obj_t* root) noexcept
    : ready_root_(make_screen_root(root)), active_root_(make_screen_root(root)),
      destination_root_(make_screen_root(root)),
      ready_screen_(ready_root_, ready_navigation, this), active_screen_(active_root_)
{
    destination_title_ = ui::create_label(destination_root_, ui::Typography::heading,
                                          ui::color::text_primary);
    lv_obj_set_pos(destination_title_, 20, 70);
    lv_obj_set_size(destination_title_, 560, 40);

    destination_message_ = ui::create_label(destination_root_, ui::Typography::body,
                                            ui::color::text_secondary);
    lv_label_set_text(destination_message_, "Screen content is delivered by its linked issue");
    lv_obj_set_pos(destination_message_, 40, 155);
    lv_obj_set_size(destination_message_, 520, 70);
    lv_label_set_long_mode(destination_message_, LV_LABEL_LONG_WRAP);

    back_button_ = lv_button_create(destination_root_);
    ui::style_flat_panel(back_button_, ui::color::surface, 12);
    lv_obj_set_pos(back_button_, 170, 330);
    lv_obj_set_size(back_button_, 260, 80);
    lv_obj_add_flag(back_button_, LV_OBJ_FLAG_EVENT_BUBBLE);
    auto* back_label = ui::create_label(back_button_, ui::Typography::body,
                                       ui::color::text_primary);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " BACK TO READY");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_button_, back_event, LV_EVENT_CLICKED, this);

    show_destination();
}

void ApplicationScreen::update(const ui::ReadyViewModel& ready,
                               const ui::DeviceViewModel& active) noexcept
{
    ready_screen_.update(ready);
    active_screen_.update(active);
}

ui::NavigationResult ApplicationScreen::navigate(const ui::NavigationAction action) noexcept
{
    const auto result = navigation_.dispatch(action);
    start_requested_ = start_requested_ || result.start_requested;
    show_destination();
    return result;
}

void ApplicationScreen::synchronize_session(const bool active) noexcept
{
    navigation_.synchronize_session(active);
    show_destination();
}

bool ApplicationScreen::consume_start_request() noexcept
{
    const auto requested = start_requested_;
    start_requested_ = false;
    return requested;
}

void ApplicationScreen::add_controls_to_group(lv_group_t* group) noexcept
{
    ready_screen_.add_buttons_to_group(group);
    lv_group_add_obj(group, back_button_);
}

ui::Destination ApplicationScreen::destination() const noexcept
{
    return navigation_.destination();
}

ui::ReadyScreen& ApplicationScreen::ready_screen() noexcept
{
    return ready_screen_;
}

lv_obj_t* ApplicationScreen::back_button_object() const noexcept
{
    return back_button_;
}

void ApplicationScreen::ready_navigation(const ui::NavigationAction action,
                                         void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen != nullptr) {
        (void)screen->navigate(action);
    }
}

void ApplicationScreen::back_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(lv_event_get_user_data(event));
    if (screen != nullptr) {
        (void)screen->navigate(ui::NavigationAction::back);
    }
}

void ApplicationScreen::show_destination() noexcept
{
    lv_obj_add_flag(ready_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(active_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(destination_root_, LV_OBJ_FLAG_HIDDEN);

    switch (navigation_.destination()) {
    case ui::Destination::ready:
        lv_obj_remove_flag(ready_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::active:
        lv_obj_remove_flag(active_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::setup:
        lv_label_set_text(destination_title_, LV_SYMBOL_SETTINGS " SETUP");
        lv_obj_remove_flag(destination_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::review:
        lv_label_set_text(destination_title_, LV_SYMBOL_LIST " REVIEW");
        lv_obj_remove_flag(destination_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::diagnostics:
        lv_label_set_text(destination_title_, LV_SYMBOL_WARNING " DIAGNOSTICS");
        lv_obj_remove_flag(destination_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

}  // namespace track_timer::simulator
