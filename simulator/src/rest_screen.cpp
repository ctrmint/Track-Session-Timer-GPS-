#include "rest_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::simulator {
namespace {

constexpr std::size_t kRestCellCount = 5;
constexpr FixedCellWidths kRestCellWidths{34, 34, 16, 34, 34};

}  // namespace

RestScreen::RestScreen(lv_obj_t* root, const RestScreenCallback callback,
                       void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    ui::style_screen(root_);

    auto* banner = lv_obj_create(root_);
    ui::style_flat_panel(banner, ui::color::overtime);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 600, 88);
    title_ = ui::create_label(banner, ui::Typography::heading,
                              ui::contrast_text_rgb(ui::color::overtime));
    lv_label_set_text(title_, "REST / RECOVERY");
    lv_obj_center(title_);

    auto* caption = ui::create_label(root_, ui::Typography::caption,
                                     ui::color::text_secondary);
    lv_label_set_text(caption, "REST REMAINING");
    lv_obj_set_pos(caption, 0, 118);
    lv_obj_set_size(caption, 600, 24);

    remaining_.create(root_, &lv_font_montserrat_48, lv_color_white(),
                      kRestCellCount, kRestCellWidths, 62);
    remaining_.set_position((600 - remaining_.width()) / 2, 158);

    completion_ = ui::create_label(root_, ui::Typography::body,
                                   ui::color::text_primary);
    lv_obj_set_pos(completion_, 40, 242);
    lv_obj_set_size(completion_, 520, 32);
    next_step_ = ui::create_label(root_, ui::Typography::caption,
                                  ui::color::text_secondary);
    lv_obj_set_pos(next_step_, 40, 280);
    lv_obj_set_size(next_step_, 520, 24);

    skip_button_ = lv_button_create(root_);
    ui::style_flat_panel(skip_button_, ui::color::surface, 10);
    lv_obj_set_pos(skip_button_, 40, 330);
    lv_obj_set_size(skip_button_, 520, 82);
    skip_label_ = ui::create_label(skip_button_, ui::Typography::body,
                                   ui::color::text_primary);
    lv_label_set_text(skip_label_, "HOLD TO SKIP REST");
    lv_obj_center(skip_label_);
    lv_obj_add_event_cb(skip_button_, skip_button_event, LV_EVENT_ALL, this);

    cancel_button_ = lv_button_create(root_);
    ui::style_flat_panel(cancel_button_, ui::color::surface, 10);
    lv_obj_set_pos(cancel_button_, 40, 330);
    lv_obj_set_size(cancel_button_, 250, 82);
    auto* cancel_label = ui::create_label(cancel_button_, ui::Typography::body,
                                          ui::color::text_primary);
    lv_label_set_text(cancel_label, "KEEP RESTING");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_button_, confirmation_button_event, LV_EVENT_CLICKED, this);

    confirm_button_ = lv_button_create(root_);
    ui::style_flat_panel(confirm_button_, ui::color::critical, 10);
    lv_obj_set_pos(confirm_button_, 310, 330);
    lv_obj_set_size(confirm_button_, 250, 82);
    auto* confirm_label = ui::create_label(confirm_button_, ui::Typography::body,
                                           ui::color::text_primary);
    lv_label_set_text(confirm_label, "SKIP TO READY");
    lv_obj_center(confirm_label);
    lv_obj_add_event_cb(confirm_button_, confirmation_button_event, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(cancel_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(confirm_button_, LV_OBJ_FLAG_HIDDEN);
}

void RestScreen::update(const ui::RestSessionViewModel& model) noexcept
{
    remaining_.set_text(model.remaining.data());
    lv_label_set_text(completion_, model.completion.data());
    lv_label_set_text(next_step_, model.next_step.data());
    lv_label_set_text(skip_label_, model.skip.hold_label.data());
    if (model.skip.hold_visible) {
        lv_obj_remove_flag(skip_button_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(skip_button_, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto* button : {cancel_button_, confirm_button_}) {
        if (model.skip.confirmation_visible) {
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (model.skip.state == ui::StopControlState::requested) {
        lv_obj_add_state(skip_button_, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(skip_button_, LV_STATE_DISABLED);
    }
}

void RestScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    lv_group_add_obj(group, skip_button_);
    lv_group_add_obj(group, cancel_button_);
    lv_group_add_obj(group, confirm_button_);
}

lv_obj_t* RestScreen::button_for(const RestScreenAction action) const noexcept
{
    switch (action) {
    case RestScreenAction::press_skip:
    case RestScreenAction::release_skip:
    case RestScreenAction::cancel_skip_hold:
        return skip_button_;
    case RestScreenAction::cancel_skip:
        return cancel_button_;
    case RestScreenAction::confirm_skip:
        return confirm_button_;
    }
    return nullptr;
}

lv_obj_t* RestScreen::title_object() const noexcept
{
    return title_;
}

lv_obj_t* RestScreen::completion_object() const noexcept
{
    return completion_;
}

lv_obj_t* RestScreen::remaining_object() const noexcept
{
    return remaining_.object();
}

void RestScreen::skip_button_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<RestScreen*>(lv_event_get_user_data(event));
    if (screen == nullptr) {
        return;
    }
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        screen->emit(RestScreenAction::press_skip);
        break;
    case LV_EVENT_RELEASED:
        screen->emit(RestScreenAction::release_skip);
        break;
    case LV_EVENT_PRESS_LOST:
        screen->emit(RestScreenAction::cancel_skip_hold);
        break;
    default:
        break;
    }
}

void RestScreen::confirmation_button_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<RestScreen*>(lv_event_get_user_data(event));
    auto* target = lv_event_get_target_obj(event);
    if (screen == nullptr || target == nullptr) {
        return;
    }
    screen->emit(target == screen->confirm_button_ ? RestScreenAction::confirm_skip
                                                   : RestScreenAction::cancel_skip);
}

void RestScreen::emit(const RestScreenAction action) noexcept
{
    if (callback_ != nullptr) {
        callback_(action, context_);
    }
}

}  // namespace track_timer::simulator
