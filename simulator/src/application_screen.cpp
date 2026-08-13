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
                                     const track::TrackMatchResult& track_match,
                                     logger::SessionSummaryProvider* summary_provider) noexcept
    : settings_manager_(settings_manager), track_catalog_(track_catalog),
      track_match_(track_match), summary_provider_(summary_provider),
      ready_root_(make_screen_root(root)),
      active_root_(make_screen_root(root)), setup_menu_root_(make_screen_root(root)),
      settings_root_(make_screen_root(root)), track_selection_root_(make_screen_root(root)),
      session_review_root_(make_screen_root(root)),
      diagnostics_root_(make_screen_root(root)),
      ready_screen_(ready_root_, ready_navigation, this),
      active_screen_(active_root_, device_action, this),
      setup_menu_screen_(setup_menu_root_, setup_action, this),
      settings_screen_(settings_root_, settings_action, this),
      track_selection_screen_(track_selection_root_, track_action, this),
      session_review_screen_(session_review_root_, review_action, this),
      diagnostics_screen_(diagnostics_root_, diagnostics_action, this)
{
    show_destination();
}

void ApplicationScreen::update(const ui::ReadyViewModel& ready,
                               const domain::UiSnapshot& active,
                               const diagnostics::DiagnosticsSnapshot& diagnostics,
                               const std::uint64_t now_ms) noexcept
{
    active_now_ms_ = now_ms;
    ready_screen_.update(ready);
    active_session_.update(active, active_now_ms_);
    active_screen_.update(active_session_.view_model());
    diagnostics_snapshot_ = diagnostics;
    diagnostics_.update(diagnostics_snapshot_);
    if (navigation_.destination() == ui::Destination::diagnostics) {
        refresh_diagnostics();
    }
}

ui::NavigationResult ApplicationScreen::navigate(const ui::NavigationAction action) noexcept
{
    const auto previous = navigation_.destination();
    const auto result = navigation_.dispatch(action);
    start_requested_ = start_requested_ || result.start_requested;
    if (result.accepted && action == ui::NavigationAction::open_setup) {
        setup_page_ = SetupPage::menu;
    }
    if (result.accepted && action == ui::NavigationAction::open_review) {
        session_review_.begin(summary_provider_);
        refresh_session_review();
    }
    else if (result.accepted && action == ui::NavigationAction::open_diagnostics) {
        diagnostics_.begin(diagnostics_snapshot_);
        refresh_diagnostics();
    }
    else if (previous == ui::Destination::review &&
             result.current != ui::Destination::review) {
        session_review_.close();
    }
    else if (previous == ui::Destination::diagnostics &&
             result.current != ui::Destination::diagnostics) {
        diagnostics_.close();
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

bool ApplicationScreen::consume_stop_request() noexcept
{
    return active_session_.consume_stop_request();
}

bool ApplicationScreen::consume_rest_request() noexcept
{
    const auto requested = rest_requested_;
    rest_requested_ = false;
    return requested;
}

bool ApplicationScreen::consume_ready_request() noexcept
{
    const auto requested = ready_requested_;
    ready_requested_ = false;
    return requested;
}

void ApplicationScreen::add_controls_to_group(lv_group_t* group) noexcept
{
    ready_screen_.add_buttons_to_group(group);
    active_screen_.add_buttons_to_group(group);
    setup_menu_screen_.add_buttons_to_group(group);
    settings_screen_.add_buttons_to_group(group);
    track_selection_screen_.add_buttons_to_group(group);
    session_review_screen_.add_buttons_to_group(group);
    diagnostics_screen_.add_buttons_to_group(group);
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

ui::SessionReviewScreen& ApplicationScreen::session_review_screen() noexcept
{
    return session_review_screen_;
}

const ui::SessionReviewController& ApplicationScreen::session_review() const noexcept
{
    return session_review_;
}

ui::DiagnosticsScreen& ApplicationScreen::diagnostics_screen() noexcept
{
    return diagnostics_screen_;
}

const ui::DiagnosticsController& ApplicationScreen::diagnostics() const noexcept
{
    return diagnostics_;
}

DeviceScreen& ApplicationScreen::device_screen() noexcept
{
    return active_screen_;
}

const ui::ActiveSessionController& ApplicationScreen::active_session() const noexcept
{
    return active_session_;
}

SetupPage ApplicationScreen::setup_page() const noexcept
{
    return setup_page_;
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

void ApplicationScreen::review_action(const ui::SessionReviewAction action,
                                      void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    switch (action) {
    case ui::SessionReviewAction::newer_session:
        screen->session_review_.newer_session();
        break;
    case ui::SessionReviewAction::older_session:
        screen->session_review_.older_session();
        break;
    case ui::SessionReviewAction::previous_page:
        screen->session_review_.previous_lap_page();
        break;
    case ui::SessionReviewAction::next_page:
        screen->session_review_.next_lap_page();
        break;
    case ui::SessionReviewAction::return_to_rest:
        screen->rest_requested_ = true;
        (void)screen->navigate(ui::NavigationAction::back);
        return;
    case ui::SessionReviewAction::return_to_ready:
        screen->ready_requested_ = true;
        (void)screen->navigate(ui::NavigationAction::back);
        return;
    }
    screen->refresh_session_review();
}

void ApplicationScreen::diagnostics_action(const ui::DiagnosticsAction action,
                                           void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    switch (action) {
    case ui::DiagnosticsAction::previous_page:
        screen->diagnostics_.previous_page();
        break;
    case ui::DiagnosticsAction::next_page:
        screen->diagnostics_.next_page();
        break;
    case ui::DiagnosticsAction::back:
        (void)screen->navigate(ui::NavigationAction::back);
        return;
    }
    screen->refresh_diagnostics();
}

void ApplicationScreen::device_action(const DeviceScreenAction action, void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr || !screen->session_active_) {
        return;
    }
    switch (action) {
    case DeviceScreenAction::press_stop:
        screen->active_session_.press_stop(screen->active_now_ms_);
        break;
    case DeviceScreenAction::release_stop:
        screen->active_session_.release_stop(screen->active_now_ms_);
        break;
    case DeviceScreenAction::cancel_stop_hold:
        screen->active_session_.cancel_stop_hold(screen->active_now_ms_);
        break;
    case DeviceScreenAction::cancel_stop:
        screen->active_session_.cancel_stop(screen->active_now_ms_);
        break;
    case DeviceScreenAction::confirm_stop:
        screen->active_session_.confirm_stop(screen->active_now_ms_);
        break;
    }
    screen->active_screen_.update(screen->active_session_.view_model());
}

void ApplicationScreen::show_destination() noexcept
{
    lv_obj_add_flag(ready_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(active_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(setup_menu_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(track_selection_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(session_review_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(diagnostics_root_, LV_OBJ_FLAG_HIDDEN);

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
        lv_obj_remove_flag(session_review_root_, LV_OBJ_FLAG_HIDDEN);
        break;
    case ui::Destination::diagnostics:
        lv_obj_remove_flag(diagnostics_root_, LV_OBJ_FLAG_HIDDEN);
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

void ApplicationScreen::refresh_session_review() noexcept
{
    session_review_screen_.update(session_review_.view_model());
}

void ApplicationScreen::refresh_diagnostics() noexcept
{
    diagnostics_screen_.update(diagnostics_.view_model());
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
