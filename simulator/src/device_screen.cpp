#include "device_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::simulator {
namespace {

constexpr std::size_t kLapTimeCellCount = 10;
constexpr std::size_t kSessionTimeCellCount = 7;
constexpr auto kLargeLapTimeCellWidths = lap_time_cell_widths(34, 16, 12);
constexpr auto kSmallLapTimeCellWidths = lap_time_cell_widths(20, 10, 8);
constexpr auto kSessionTimeCellWidths = session_time_cell_widths(34, 16);
constexpr std::uint32_t gnss_indicator_color(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::good:
        return ui::color::positive_bright;
    case domain::GnssHealth::poor:
        return ui::color::caution_bright;
    case domain::GnssHealth::unavailable:
    case domain::GnssHealth::searching:
    case domain::GnssHealth::stale:
        return ui::color::critical_bright;
    }
    return ui::color::critical_bright;
}

const char* gnss_indicator_text(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::unavailable:
        return LV_SYMBOL_GPS " NO GPS";
    case domain::GnssHealth::searching:
        return LV_SYMBOL_GPS " SEARCH";
    case domain::GnssHealth::poor:
        return LV_SYMBOL_GPS " POOR";
    case domain::GnssHealth::good:
        return LV_SYMBOL_GPS " GOOD";
    case domain::GnssHealth::stale:
        return LV_SYMBOL_GPS " STALE";
    }
    return LV_SYMBOL_GPS " NO GPS";
}

constexpr std::int32_t centered_lap_field_x(const std::int32_t area_x,
                                            const std::int32_t area_width,
                                            const std::int32_t total_width,
                                            const std::int32_t digit_width) noexcept
{
    constexpr std::int32_t kReservedLeadingCells = 2;
    const auto leading_width = kReservedLeadingCells * digit_width;
    const auto common_value_width = total_width - leading_width;
    return area_x + (area_width - common_value_width) / 2 - leading_width;
}

}  // namespace

DeviceScreen::DeviceScreen(lv_obj_t* root, const DeviceScreenCallback callback,
                           void* callback_context) noexcept
    : callback_(callback), callback_context_(callback_context), root_(root)
{
    ui::style_screen(root_);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    lap_label_ = ui::create_label(root_, ui::Typography::heading, ui::color::text_primary,
                                  LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(lap_label_, 20, 18);
    lv_obj_set_size(lap_label_, 170, 34);

    gnss_label_ = ui::create_label(root_, ui::Typography::body,
                                   ui::color::critical_bright, LV_TEXT_ALIGN_RIGHT);
    lv_label_set_text(gnss_label_, gnss_indicator_text(domain::GnssHealth::unavailable));
    lv_obj_set_pos(gnss_label_, 370, 21);
    lv_obj_set_size(gnss_label_, 210, 30);

    accent_line_ = lv_obj_create(root_);
    ui::style_flat_panel(accent_line_, ui::color::surface);
    lv_obj_set_pos(accent_line_, 0, 62);
    lv_obj_set_size(accent_line_, 600, 6);

    current_caption_ = ui::create_label(root_, ui::Typography::caption,
                                        ui::color::text_secondary);
    lv_label_set_text(current_caption_, "CURRENT LAP");
    lv_obj_set_pos(current_caption_, 0, 86);
    lv_obj_set_size(current_caption_, 600, 24);

    current_lap_label_.create(root_, &lv_font_montserrat_48, lv_color_white(),
                              kLapTimeCellCount, kLargeLapTimeCellWidths, 62);
    current_lap_label_.set_position(
        centered_lap_field_x(0, 600, current_lap_label_.width(), 34), 112);

    feedback_panel_ = lv_obj_create(root_);
    ui::style_flat_panel(feedback_panel_, ui::color::surface, 12);
    lv_obj_set_pos(feedback_panel_, 20, 78);
    lv_obj_set_size(feedback_panel_, 560, 116);
    feedback_heading_ = ui::create_label(feedback_panel_, ui::Typography::caption,
                                         ui::color::text_secondary);
    lv_obj_set_pos(feedback_heading_, 18, 9);
    lv_obj_set_size(feedback_heading_, 170, 22);
    feedback_time_ = ui::create_label(feedback_panel_, ui::Typography::timer_secondary,
                                      ui::color::text_primary);
    lv_obj_set_pos(feedback_time_, 18, 40);
    lv_obj_set_size(feedback_time_, 250, 42);
    feedback_comparison_ = ui::create_label(feedback_panel_, ui::Typography::body,
                                            ui::color::caution_bright,
                                            LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(feedback_comparison_, 270, 44);
    lv_obj_set_size(feedback_comparison_, 270, 34);
    lv_obj_add_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);

    previous_caption_ = ui::create_label(root_, ui::Typography::caption,
                                         ui::color::text_secondary);
    lv_label_set_text(previous_caption_, "LAST");
    lv_obj_set_pos(previous_caption_, 30, 210);
    lv_obj_set_size(previous_caption_, 250, 22);

    previous_lap_label_.create(root_, &lv_font_montserrat_28, lv_color_white(),
                               kLapTimeCellCount, kSmallLapTimeCellWidths, 38);
    previous_lap_label_.set_position(
        centered_lap_field_x(30, 250, previous_lap_label_.width(), 20), 236);

    best_caption_ = ui::create_label(root_, ui::Typography::caption,
                                     ui::color::text_secondary);
    lv_label_set_text(best_caption_, "BEST");
    lv_obj_set_pos(best_caption_, 320, 210);
    lv_obj_set_size(best_caption_, 250, 22);

    best_lap_label_.create(root_, &lv_font_montserrat_28, lv_color_white(),
                           kLapTimeCellCount, kSmallLapTimeCellWidths, 38);
    best_lap_label_.set_position(
        centered_lap_field_x(320, 250, best_lap_label_.width(), 20), 236);

    trackday_panel_ = lv_obj_create(root_);
    ui::style_flat_panel(trackday_panel_, ui::color::background);
    lv_obj_set_pos(trackday_panel_, 0, 72);
    lv_obj_set_size(trackday_panel_, 600, 250);
    auto* trackday_title = ui::create_label(trackday_panel_, ui::Typography::heading,
                                            ui::color::text_primary);
    lv_label_set_text(trackday_title, "TRACKDAY MODE");
    lv_obj_set_pos(trackday_title, 0, 8);
    lv_obj_set_size(trackday_title, 600, 34);
    auto* countdown_caption = ui::create_label(trackday_panel_, ui::Typography::caption,
                                               ui::color::text_secondary);
    lv_label_set_text(countdown_caption, "SESSION REMAINING");
    lv_obj_set_pos(countdown_caption, 0, 48);
    lv_obj_set_size(countdown_caption, 600, 24);
    trackday_countdown_.create(trackday_panel_, &lv_font_montserrat_48,
                               lv_color_white(), kSessionTimeCellCount,
                               kSessionTimeCellWidths, 62);
    trackday_countdown_.set_position((600 - trackday_countdown_.width()) / 2, 76);
    auto* estimate_caption = ui::create_label(trackday_panel_, ui::Typography::caption,
                                              ui::color::text_secondary);
    lv_label_set_text(estimate_caption, "ESTIMATED LAPS REMAINING");
    lv_obj_set_pos(estimate_caption, 0, 154);
    lv_obj_set_size(estimate_caption, 600, 24);
    trackday_estimate_ = ui::create_label(trackday_panel_, ui::Typography::heading,
                                          ui::color::text_primary);
    lv_obj_set_pos(trackday_estimate_, 0, 184);
    lv_obj_set_size(trackday_estimate_, 600, 40);
    lv_obj_add_flag(trackday_panel_, LV_OBJ_FLAG_HIDDEN);

    logging_badge_ = lv_obj_create(root_);
    ui::style_flat_panel(logging_badge_, ui::color::logging_unavailable, 14);
    lv_obj_set_pos(logging_badge_, 240, 289);
    lv_obj_set_size(logging_badge_, 120, 30);

    logging_label_ = ui::create_label(logging_badge_, ui::Typography::caption,
                                      ui::color::text_primary);
    lv_obj_center(logging_label_);

    session_panel_ = lv_obj_create(root_);
    ui::style_flat_panel(session_panel_, ui::color::surface);
    lv_obj_set_pos(session_panel_, 0, 330);
    lv_obj_set_size(session_panel_, 600, 120);

    session_caption_ = ui::create_label(session_panel_, ui::Typography::body,
                                        ui::color::text_primary, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(session_caption_, "SESSION");
    lv_obj_set_pos(session_caption_, 180, 16);
    lv_obj_set_size(session_caption_, 200, 30);

    session_label_.create(session_panel_, &lv_font_montserrat_48, lv_color_white(),
                          kSessionTimeCellCount, kSessionTimeCellWidths, 60);
    session_label_.set_position(570 - session_label_.width(), 25);

    stop_button_ = lv_button_create(session_panel_);
    ui::style_flat_panel(stop_button_, ui::color::critical, 10);
    lv_obj_set_pos(stop_button_, 16, 48);
    lv_obj_set_size(stop_button_, 148, 56);
    stop_label_ = ui::create_label(stop_button_, ui::Typography::caption,
                                   ui::color::text_primary);
    lv_obj_center(stop_label_);
    lv_obj_add_event_cb(stop_button_, stop_button_event, LV_EVENT_ALL, this);

    cancel_button_ = lv_button_create(session_panel_);
    ui::style_flat_panel(cancel_button_, ui::color::surface, 10);
    lv_obj_set_pos(cancel_button_, 16, 48);
    lv_obj_set_size(cancel_button_, 70, 56);
    auto* cancel_label = ui::create_label(cancel_button_, ui::Typography::caption,
                                          ui::color::text_primary);
    lv_label_set_text(cancel_label, "CANCEL");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_button_, confirmation_button_event, LV_EVENT_CLICKED, this);

    confirm_button_ = lv_button_create(session_panel_);
    ui::style_flat_panel(confirm_button_, ui::color::critical, 10);
    lv_obj_set_pos(confirm_button_, 94, 48);
    lv_obj_set_size(confirm_button_, 70, 56);
    auto* confirm_label = ui::create_label(confirm_button_, ui::Typography::caption,
                                           ui::color::text_primary);
    lv_label_set_text(confirm_label, "STOP");
    lv_obj_center(confirm_label);
    lv_obj_add_event_cb(confirm_button_, confirmation_button_event, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(cancel_button_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(confirm_button_, LV_OBJ_FLAG_HIDDEN);
}

void DeviceScreen::update(const ui::ActiveSessionViewModel& active) noexcept
{
    const auto& model = active.timing;
    lv_label_set_text(lap_label_, model.lap_label.data());
    lv_label_set_text(gnss_label_, gnss_indicator_text(model.gnss_health));
    lv_obj_set_style_text_color(gnss_label_, lv_color_hex(gnss_indicator_color(model.gnss_health)),
                                0);
    current_lap_label_.set_text(model.current_lap.data());
    previous_lap_label_.set_text(model.previous_lap.data());
    best_lap_label_.set_text(model.best_lap.data());
    lv_label_set_text(logging_label_, model.logging_status.data());
    lv_label_set_text(session_caption_, model.session_status.data());
    session_label_.set_text(model.session_remaining.data());
    trackday_countdown_.set_text(active.trackday.countdown.data());
    lv_label_set_text(trackday_estimate_, active.trackday.estimated_laps.data());
    lv_obj_set_style_text_color(
        trackday_estimate_,
        lv_color_hex(active.trackday.estimate_available ? ui::color::text_primary
                                                        : ui::color::caution_bright),
        0);

    const auto accent = lv_color_hex(model.accent_rgb);
    const auto accent_text = lv_color_hex(model.accent_text_rgb);
    lv_obj_set_style_bg_color(accent_line_, accent, 0);
    lv_obj_set_style_bg_color(session_panel_, accent, 0);
    lv_obj_set_style_text_color(session_caption_, accent_text, 0);
    session_label_.set_color(accent_text);

    const bool logging = model.logging_status[0] != 'N';
    lv_obj_set_style_bg_color(
        logging_badge_,
        lv_color_hex(logging ? ui::color::logging : ui::color::logging_unavailable), 0);

    if (active.trackday.visible) {
        lv_obj_remove_flag(trackday_panel_, LV_OBJ_FLAG_HIDDEN);
        for (auto* object : {current_caption_, current_lap_label_.object(),
                             previous_caption_, previous_lap_label_.object(),
                             best_caption_, best_lap_label_.object(), feedback_panel_}) {
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if (active.feedback.visible) {
        lv_obj_add_flag(trackday_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(current_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(current_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(previous_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(previous_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(best_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(best_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(feedback_heading_, active.feedback.heading.data());
        lv_label_set_text(feedback_time_, active.feedback.completed_lap.data());
        lv_label_set_text(feedback_comparison_, active.feedback.comparison.data());
        lv_obj_set_style_text_color(feedback_comparison_,
                                    lv_color_hex(active.feedback.color_rgb), 0);
    }
    else {
        lv_obj_add_flag(trackday_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(current_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(current_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(previous_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(previous_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(best_caption_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(best_lap_label_.object(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(feedback_panel_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(stop_label_, active.stop.hold_label.data());
    if (active.stop.hold_visible) {
        lv_obj_remove_flag(stop_button_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(stop_button_, LV_OBJ_FLAG_HIDDEN);
    }
    if (active.stop.state == ui::StopControlState::requested) {
        lv_obj_add_state(stop_button_, LV_STATE_DISABLED);
    }
    else {
        lv_obj_remove_state(stop_button_, LV_STATE_DISABLED);
    }
    for (auto* button : {cancel_button_, confirm_button_}) {
        if (active.stop.confirmation_visible) {
            lv_obj_remove_flag(button, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void DeviceScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    lv_group_add_obj(group, stop_button_);
    lv_group_add_obj(group, cancel_button_);
    lv_group_add_obj(group, confirm_button_);
}

lv_obj_t* DeviceScreen::button_for(const DeviceScreenAction action) const noexcept
{
    switch (action) {
    case DeviceScreenAction::press_stop:
    case DeviceScreenAction::release_stop:
    case DeviceScreenAction::cancel_stop_hold:
        return stop_button_;
    case DeviceScreenAction::cancel_stop:
        return cancel_button_;
    case DeviceScreenAction::confirm_stop:
        return confirm_button_;
    }
    return nullptr;
}

lv_obj_t* DeviceScreen::feedback_panel_object() const noexcept
{
    return feedback_panel_;
}

lv_obj_t* DeviceScreen::feedback_comparison_object() const noexcept
{
    return feedback_comparison_;
}

lv_obj_t* DeviceScreen::session_panel_object() const noexcept
{
    return session_panel_;
}

lv_obj_t* DeviceScreen::trackday_panel_object() const noexcept
{
    return trackday_panel_;
}

lv_obj_t* DeviceScreen::trackday_countdown_object() const noexcept
{
    return trackday_countdown_.object();
}

lv_obj_t* DeviceScreen::trackday_estimate_object() const noexcept
{
    return trackday_estimate_;
}

lv_obj_t* DeviceScreen::current_lap_object() const noexcept
{
    return current_lap_label_.object();
}

lv_obj_t* DeviceScreen::previous_lap_object() const noexcept
{
    return previous_lap_label_.object();
}

lv_obj_t* DeviceScreen::best_lap_object() const noexcept
{
    return best_lap_label_.object();
}

void DeviceScreen::stop_button_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<DeviceScreen*>(lv_event_get_user_data(event));
    if (screen == nullptr) {
        return;
    }
    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        screen->emit(DeviceScreenAction::press_stop);
        break;
    case LV_EVENT_RELEASED:
        screen->emit(DeviceScreenAction::release_stop);
        break;
    case LV_EVENT_PRESS_LOST:
        screen->emit(DeviceScreenAction::cancel_stop_hold);
        break;
    default:
        break;
    }
}

void DeviceScreen::confirmation_button_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<DeviceScreen*>(lv_event_get_user_data(event));
    auto* target = lv_event_get_target_obj(event);
    if (screen == nullptr || target == nullptr) {
        return;
    }
    screen->emit(target == screen->confirm_button_ ? DeviceScreenAction::confirm_stop
                                                   : DeviceScreenAction::cancel_stop);
}

void DeviceScreen::emit(const DeviceScreenAction action) noexcept
{
    if (callback_ != nullptr) {
        callback_(action, callback_context_);
    }
}

}  // namespace track_timer::simulator
