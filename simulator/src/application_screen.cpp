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

ApplicationScreen::ApplicationScreen(lv_obj_t* root,
                                     settings::SettingsManager& settings_manager,
                                     const track::TrackCatalogView track_catalog,
                                     const track::TrackMatchResult& track_match) noexcept
    : settings_manager_(settings_manager), track_catalog_(track_catalog),
      track_match_(track_match), ready_root_(make_screen_root(root)),
      active_root_(make_screen_root(root)), setup_menu_root_(make_screen_root(root)),
      settings_root_(make_screen_root(root)), track_selection_root_(make_screen_root(root)),
      destination_root_(make_screen_root(root)),
      ready_screen_(ready_root_, ready_navigation, this), active_screen_(active_root_),
      setup_menu_screen_(setup_menu_root_, setup_action, this),
      settings_screen_(settings_root_, settings_action, this),
      track_selection_screen_(track_selection_root_, track_action, this)
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
    if (result.accepted && action == ui::NavigationAction::open_setup) {
        setup_page_ = SetupPage::menu;
    }
    show_destination();
    return result;
}

void ApplicationScreen::update_track_match(const track::TrackMatchResult& match) noexcept
{
    track_match_ = match;
}

void ApplicationScreen::open_setup_page(const SetupPage page) noexcept
{
    if (navigation_.destination() != ui::Destination::setup || session_active_) {
        return;
    }
    setup_page_ = page;
    if (page == SetupPage::device_settings) {
        (void)settings_editor_.begin(settings_manager_.current(), false);
        refresh_settings();
    }
    else if (page == SetupPage::track_selection) {
        track_selection_.begin(track_catalog_, track_match_, settings_manager_.current(), false);
        refresh_track_selection();
    }
    show_destination();
}

void ApplicationScreen::synchronize_session(const bool active) noexcept
{
    const auto was_active = session_active_;
    session_active_ = active;
    navigation_.synchronize_session(active);
    if (was_active && !active) {
        (void)settings_manager_.apply_deferred(false);
    }
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
    setup_menu_screen_.add_buttons_to_group(group);
    settings_screen_.add_buttons_to_group(group);
    track_selection_screen_.add_buttons_to_group(group);
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

ui::SetupMenuScreen& ApplicationScreen::setup_menu_screen() noexcept
{
    return setup_menu_screen_;
}

ui::SettingsScreen& ApplicationScreen::settings_screen() noexcept
{
    return settings_screen_;
}

const ui::SettingsEditor& ApplicationScreen::settings_editor() const noexcept
{
    return settings_editor_;
}

ui::TrackSelectionScreen& ApplicationScreen::track_selection_screen() noexcept
{
    return track_selection_screen_;
}

const ui::TrackSelectionController& ApplicationScreen::track_selection() const noexcept
{
    return track_selection_;
}

SetupPage ApplicationScreen::setup_page() const noexcept
{
    return setup_page_;
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

void ApplicationScreen::setup_action(const ui::SetupMenuAction action, void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    switch (action) {
    case ui::SetupMenuAction::device_settings:
        screen->open_setup_page(SetupPage::device_settings);
        break;
    case ui::SetupMenuAction::track_selection:
        screen->open_setup_page(SetupPage::track_selection);
        break;
    case ui::SetupMenuAction::back:
        (void)screen->navigate(ui::NavigationAction::back);
        break;
    }
}

void ApplicationScreen::settings_action(const ui::SettingsScreenAction action,
                                        void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    switch (action) {
    case ui::SettingsScreenAction::previous_field:
        screen->settings_editor_.previous_field();
        break;
    case ui::SettingsScreenAction::next_field:
        screen->settings_editor_.next_field();
        break;
    case ui::SettingsScreenAction::decrement:
        (void)screen->settings_editor_.decrement();
        break;
    case ui::SettingsScreenAction::increment:
        (void)screen->settings_editor_.increment();
        break;
    case ui::SettingsScreenAction::save:
        (void)screen->settings_editor_.save(screen->settings_manager_, screen->session_active_);
        break;
    case ui::SettingsScreenAction::cancel:
        screen->settings_editor_.cancel();
        screen->setup_page_ = SetupPage::menu;
        screen->show_destination();
        return;
    case ui::SettingsScreenAction::restore_defaults:
        screen->settings_editor_.request_restore_defaults();
        break;
    case ui::SettingsScreenAction::confirm_defaults:
        screen->settings_editor_.resolve_restore_defaults(true);
        break;
    case ui::SettingsScreenAction::cancel_defaults:
        screen->settings_editor_.resolve_restore_defaults(false);
        break;
    }
    screen->refresh_settings();
}

void ApplicationScreen::track_action(const ui::TrackSelectionAction action,
                                     void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    switch (action) {
    case ui::TrackSelectionAction::previous:
        screen->track_selection_.previous();
        break;
    case ui::TrackSelectionAction::select:
        (void)screen->track_selection_.select(screen->settings_manager_,
                                              screen->session_active_);
        break;
    case ui::TrackSelectionAction::next:
        screen->track_selection_.next();
        break;
    case ui::TrackSelectionAction::timer_only:
        (void)screen->track_selection_.use_timer_only(screen->settings_manager_,
                                                      screen->session_active_);
        break;
    case ui::TrackSelectionAction::capture_information:
        screen->track_selection_.show_capture_information();
        break;
    case ui::TrackSelectionAction::back:
        screen->track_selection_.cancel();
        screen->setup_page_ = SetupPage::menu;
        screen->show_destination();
        return;
    }
    screen->refresh_track_selection();
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
    lv_obj_add_flag(setup_menu_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(track_selection_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(destination_root_, LV_OBJ_FLAG_HIDDEN);

    switch (navigation_.destination()) {
    case ui::Destination::ready:
        lv_obj_remove_flag(ready_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::active:
        lv_obj_remove_flag(active_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::setup:
        if (setup_page_ == SetupPage::device_settings) {
            lv_obj_remove_flag(settings_root_, LV_OBJ_FLAG_HIDDEN);
        }
        else if (setup_page_ == SetupPage::track_selection) {
            lv_obj_remove_flag(track_selection_root_, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_remove_flag(setup_menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
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

void ApplicationScreen::refresh_settings() noexcept
{
    settings_screen_.update(settings_editor_.view_model());
}

void ApplicationScreen::refresh_track_selection() noexcept
{
    track_selection_screen_.update(track_selection_.view_model());
}

const char* setup_page_name(const SetupPage page) noexcept
{
    switch (page) {
    case SetupPage::menu:
        return "menu";
    case SetupPage::device_settings:
        return "settings";
    case SetupPage::track_selection:
        return "tracks";
    }
    return "menu";
}

}  // namespace track_timer::simulator
