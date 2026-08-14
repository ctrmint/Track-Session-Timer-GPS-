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
                                     logger::SessionSummaryProvider* summary_provider,
                                     board::DisplayOutput* display_output,
                                     track::TrackDefinitionStore* track_store) noexcept
    : settings_manager_(settings_manager), track_catalog_(track_catalog),
      track_match_(track_match), summary_provider_(summary_provider),
      display_output_(display_output), track_store_(track_store),
      ready_root_(make_screen_root(root)),
      active_root_(make_screen_root(root)), setup_menu_root_(make_screen_root(root)),
      settings_root_(make_screen_root(root)), track_selection_root_(make_screen_root(root)),
      gate_capture_root_(make_screen_root(root)),
      session_review_root_(make_screen_root(root)),
      diagnostics_root_(make_screen_root(root)),
      g_meter_root_(make_screen_root(root)),
      rest_root_(make_screen_root(root)),
      ready_screen_(ready_root_, ready_navigation, this),
      active_screen_(active_root_, device_action, this),
      setup_menu_screen_(setup_menu_root_, setup_action, this),
      settings_screen_(settings_root_, settings_action, this),
      track_selection_screen_(track_selection_root_, track_action, this),
      gate_capture_screen_(gate_capture_root_, gate_capture_action, this),
      session_review_screen_(session_review_root_, review_action, this),
      diagnostics_screen_(diagnostics_root_, diagnostics_action, this),
      g_meter_screen_(g_meter_root_, g_meter_action, this),
      rest_screen_(rest_root_, rest_action, this)
{
    ui::style_screen(root);
    brightness_overlay_ = lv_obj_create(root);
    ui::style_flat_panel(brightness_overlay_, ui::color::background);
    lv_obj_set_pos(brightness_overlay_, 0, 0);
    lv_obj_set_size(brightness_overlay_, 600, 450);
    lv_obj_clear_flag(brightness_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(brightness_overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ready_root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ready_root_, activity_event, LV_EVENT_PRESSED, this);
    show_destination();
}

void ApplicationScreen::update(const ui::ReadyViewModel& ready,
                               const domain::UiSnapshot& active,
                               const diagnostics::DiagnosticsSnapshot& diagnostics,
                               const std::uint64_t now_ms,
                               const ui::DisplayPolicyInput& display,
                               const settings::DeviceSettings* display_settings_override,
                               const ui::ImuMeterInput& imu,
                               const settings::DeviceSettings* active_settings_override) noexcept
{
    active_now_ms_ = now_ms;
    ready_screen_.update(ready);
    const auto& active_settings = active_settings_override == nullptr
                                      ? settings_manager_.current()
                                      : *active_settings_override;
    active_session_.update(
        active, active_now_ms_,
        {active_settings.average_lap_seconds,
         active_settings.trackday_mode_enabled});
    active_screen_.update(active_session_.view_model());
    auto policy_input = display;
    policy_input.now_ms = now_ms;
    policy_input.context = session_active_ ? ui::DisplayContext::active
                                           : navigation_.destination() == ui::Destination::ready
                                                 ? ui::DisplayContext::ready
                                                 : ui::DisplayContext::other;
    if (display_settings_override != nullptr) {
        policy_input.settings = *display_settings_override;
        policy_input.settings_preview = false;
    }
    else {
        policy_input.settings = settings_manager_.current();
        const auto editor_status = settings_editor_.status();
        const auto preview = navigation_.destination() == ui::Destination::setup &&
                             setup_page_ == SetupPage::device_settings &&
                             (editor_status == ui::SettingsEditorStatus::editing ||
                              editor_status == ui::SettingsEditorStatus::confirm_defaults ||
                              editor_status == ui::SettingsEditorStatus::defaults_staged ||
                              editor_status == ui::SettingsEditorStatus::invalid) &&
                             settings_editor_.changed();
        if (preview) {
            policy_input.settings = settings_editor_.draft();
            policy_input.settings_preview = true;
        }
    }
    policy_input.user_activity = policy_input.user_activity || activity_pending_;
    activity_pending_ = false;
    const auto& policy = display_policy_.update(policy_input);
    apply_display_policy(policy.command);
    if (display_output_ != nullptr) {
        display_output_->apply(policy.command);
    }

    auto effective_imu = imu;
    effective_imu.now_ms = now_ms;
    effective_imu.orientation = policy.command.orientation;
    g_meter_.update(effective_imu, session_active_);
    g_meter_screen_.update(g_meter_.snapshot());

    diagnostics_snapshot_ = diagnostics;
    diagnostics_snapshot_.display_brightness_percent = policy.command.brightness_percent;
    diagnostics_snapshot_.display_orientation_degrees =
        ui::display_orientation_degrees(policy.command.orientation);
    diagnostics_snapshot_.display_shift_x = policy.command.layout_shift_x;
    diagnostics_snapshot_.display_shift_y = policy.command.layout_shift_y;
    diagnostics_snapshot_.display_dimmed = policy.command.dimmed;
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

void ApplicationScreen::update_capture_fix(
    const domain::GnssFix& fix,
    const std::int64_t evaluation_monotonic_us) noexcept
{
    gate_capture_.update_fix(fix, evaluation_monotonic_us);
    if (navigation_.destination() == ui::Destination::setup &&
        setup_page_ == SetupPage::gate_capture) {
        refresh_gate_capture();
    }
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
    else if (page == SetupPage::gate_capture) {
        refresh_gate_capture();
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

void ApplicationScreen::synchronize_workflow(const session::SessionSnapshot& snapshot,
                                             const std::uint64_t now_ms) noexcept
{
    active_now_ms_ = now_ms;
    rest_session_.update(snapshot, now_ms);
    rest_screen_.update(rest_session_.view_model());
    switch (snapshot.state) {
    case session::SessionState::running:
    case session::SessionState::overtime:
        synchronize_session(true);
        return;
    case session::SessionState::review:
        synchronize_session(false);
        if (navigation_.destination() != ui::Destination::review) {
            (void)navigate(ui::NavigationAction::open_review);
        }
        return;
    case session::SessionState::rest:
        synchronize_session(false);
        if (navigation_.destination() != ui::Destination::rest) {
            (void)navigation_.dispatch(ui::NavigationAction::rest_started);
            show_destination();
        }
        return;
    case session::SessionState::ready:
        if (navigation_.destination() == ui::Destination::active ||
            navigation_.destination() == ui::Destination::rest) {
            (void)navigation_.dispatch(ui::NavigationAction::session_ended);
            show_destination();
        }
        else {
            synchronize_session(false);
        }
        return;
    case session::SessionState::configuring:
        synchronize_session(false);
        return;
    }
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

bool ApplicationScreen::consume_skip_rest_request() noexcept
{
    const auto requested = skip_rest_requested_ || rest_session_.consume_skip_request();
    skip_rest_requested_ = false;
    return requested;
}

void ApplicationScreen::add_controls_to_group(lv_group_t* group) noexcept
{
    ready_screen_.add_buttons_to_group(group);
    active_screen_.add_buttons_to_group(group);
    setup_menu_screen_.add_buttons_to_group(group);
    settings_screen_.add_buttons_to_group(group);
    track_selection_screen_.add_buttons_to_group(group);
    gate_capture_screen_.add_buttons_to_group(group);
    session_review_screen_.add_buttons_to_group(group);
    diagnostics_screen_.add_buttons_to_group(group);
    g_meter_screen_.add_buttons_to_group(group);
    rest_screen_.add_buttons_to_group(group);
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

ui::GateCaptureScreen& ApplicationScreen::gate_capture_screen() noexcept
{
    return gate_capture_screen_;
}

const ui::GateCaptureController& ApplicationScreen::gate_capture() const noexcept
{
    return gate_capture_;
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

ui::GmeterScreen& ApplicationScreen::g_meter_screen() noexcept
{
    return g_meter_screen_;
}

const ui::ImuMeterController& ApplicationScreen::g_meter() const noexcept
{
    return g_meter_;
}

RestScreen& ApplicationScreen::rest_screen() noexcept
{
    return rest_screen_;
}

const ui::RestSessionController& ApplicationScreen::rest_session() const noexcept
{
    return rest_session_;
}

DeviceScreen& ApplicationScreen::device_screen() noexcept
{
    return active_screen_;
}

const ui::ActiveSessionController& ApplicationScreen::active_session() const noexcept
{
    return active_session_;
}

const ui::DisplayPolicyController& ApplicationScreen::display_policy() const noexcept
{
    return display_policy_;
}

lv_obj_t* ApplicationScreen::brightness_overlay_object() const noexcept
{
    return brightness_overlay_;
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
        screen->activity_pending_ = true;
        (void)screen->navigate(action);
    }
}

void ApplicationScreen::setup_action(const ui::SetupMenuAction action, void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    screen->activity_pending_ = true;
    switch (action) {
    case ui::SetupMenuAction::device_settings:
        screen->open_setup_page(SetupPage::device_settings);
        break;
    case ui::SetupMenuAction::track_selection:
        screen->open_setup_page(SetupPage::track_selection);
        break;
    case ui::SetupMenuAction::g_meter:
        screen->open_setup_page(SetupPage::g_meter);
        break;
    case ui::SetupMenuAction::back:
        (void)screen->navigate(ui::NavigationAction::back);
        break;
    }
}

void ApplicationScreen::g_meter_action(const ui::GmeterAction action,
                                       void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    screen->activity_pending_ = true;
    switch (action) {
    case ui::GmeterAction::reset:
        (void)screen->g_meter_.reset(screen->session_active_);
        screen->g_meter_screen_.update(screen->g_meter_.snapshot());
        break;
    case ui::GmeterAction::back:
        screen->setup_page_ = SetupPage::menu;
        screen->show_destination();
        break;
    }
}

void ApplicationScreen::rest_action(const RestScreenAction action, void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr || screen->navigation_.destination() != ui::Destination::rest) {
        return;
    }
    screen->activity_pending_ = true;
    switch (action) {
    case RestScreenAction::press_skip:
        screen->rest_session_.press_skip(screen->active_now_ms_);
        break;
    case RestScreenAction::release_skip:
        screen->rest_session_.release_skip(screen->active_now_ms_);
        break;
    case RestScreenAction::cancel_skip_hold:
        screen->rest_session_.cancel_skip_hold(screen->active_now_ms_);
        break;
    case RestScreenAction::cancel_skip:
        screen->rest_session_.cancel_skip(screen->active_now_ms_);
        break;
    case RestScreenAction::confirm_skip:
        screen->rest_session_.confirm_skip(screen->active_now_ms_);
        screen->skip_rest_requested_ = screen->rest_session_.consume_skip_request();
        break;
    }
    screen->rest_screen_.update(screen->rest_session_.view_model());
}

void ApplicationScreen::settings_action(const ui::SettingsScreenAction action,
                                        void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    screen->activity_pending_ = true;
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
    screen->activity_pending_ = true;
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
        if (screen->track_selection_.browse_index() < screen->track_catalog_.count) {
            screen->gate_capture_.begin(
                screen->track_catalog_.definitions[screen->track_selection_.browse_index()],
                screen->track_store_, screen->session_active_);
            screen->setup_page_ = SetupPage::gate_capture;
            screen->refresh_gate_capture();
            screen->show_destination();
            return;
        }
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

void ApplicationScreen::gate_capture_action(const ui::GateCaptureAction action,
                                            void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    screen->activity_pending_ = true;
    switch (action) {
    case ui::GateCaptureAction::previous_gate:
        screen->gate_capture_.previous_gate();
        break;
    case ui::GateCaptureAction::next_gate:
        screen->gate_capture_.next_gate();
        break;
    case ui::GateCaptureAction::toggle_endpoint:
        screen->gate_capture_.toggle_endpoint();
        break;
    case ui::GateCaptureAction::capture:
        screen->gate_capture_.capture(screen->session_active_);
        break;
    case ui::GateCaptureAction::save:
        screen->gate_capture_.save(
            screen->gate_capture_.status() ==
            ui::GateCaptureStatus::overwrite_confirmation);
        break;
    case ui::GateCaptureAction::cancel:
        screen->gate_capture_.cancel();
        screen->setup_page_ = SetupPage::track_selection;
        screen->refresh_track_selection();
        screen->show_destination();
        return;
    }
    screen->refresh_gate_capture();
}

void ApplicationScreen::review_action(const ui::SessionReviewAction action,
                                      void* context) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(context);
    if (screen == nullptr) {
        return;
    }
    screen->activity_pending_ = true;
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
    screen->activity_pending_ = true;
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
    screen->activity_pending_ = true;
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

void ApplicationScreen::activity_event(lv_event_t* event) noexcept
{
    auto* screen = static_cast<ApplicationScreen*>(lv_event_get_user_data(event));
    if (screen != nullptr) {
        screen->activity_pending_ = true;
    }
}

void ApplicationScreen::show_destination() noexcept
{
    lv_obj_add_flag(ready_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(active_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(setup_menu_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(track_selection_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gate_capture_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(session_review_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(diagnostics_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_meter_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rest_root_, LV_OBJ_FLAG_HIDDEN);

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
        else if (setup_page_ == SetupPage::gate_capture) {
            lv_obj_remove_flag(gate_capture_root_, LV_OBJ_FLAG_HIDDEN);
        }
        else if (setup_page_ == SetupPage::g_meter) {
            lv_obj_remove_flag(g_meter_root_, LV_OBJ_FLAG_HIDDEN);
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
    case ui::Destination::rest:
        lv_obj_remove_flag(rest_root_, LV_OBJ_FLAG_HIDDEN);
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

void ApplicationScreen::refresh_gate_capture() noexcept
{
    gate_capture_screen_.update(gate_capture_.view_model());
}

void ApplicationScreen::refresh_session_review() noexcept
{
    session_review_screen_.update(session_review_.view_model());
}

void ApplicationScreen::refresh_diagnostics() noexcept
{
    diagnostics_screen_.update(diagnostics_.view_model());
}

void ApplicationScreen::apply_display_policy(const board::DisplayCommand& command) noexcept
{
    const auto degrees = ui::display_orientation_degrees(command.orientation);
    const auto portrait = command.orientation == board::DisplayOrientation::degrees_90 ||
                          command.orientation == board::DisplayOrientation::degrees_270;
    const auto scale = portrait ? 192 : 256;
    for (auto* root : {ready_root_, active_root_, setup_menu_root_, settings_root_,
                       track_selection_root_, gate_capture_root_, session_review_root_, diagnostics_root_,
                       g_meter_root_, rest_root_}) {
        lv_obj_set_pos(root, command.layout_shift_x, command.layout_shift_y);
        lv_obj_set_style_transform_pivot_x(root, 300, 0);
        lv_obj_set_style_transform_pivot_y(root, 225, 0);
        lv_obj_set_style_transform_rotation(root, static_cast<std::int32_t>(degrees) * 10, 0);
        lv_obj_set_style_transform_scale(root, scale, 0);
    }

    if (command.brightness_percent >= 100) {
        lv_obj_add_flag(brightness_overlay_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        const auto opacity = static_cast<lv_opa_t>(
            (100U - command.brightness_percent) * static_cast<unsigned>(LV_OPA_COVER) / 100U);
        lv_obj_set_style_bg_opa(brightness_overlay_, opacity, 0);
        lv_obj_remove_flag(brightness_overlay_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(brightness_overlay_);
    }
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
    case SetupPage::gate_capture:
        return "gate-capture";
    case SetupPage::g_meter:
        return "g-meter";
    }
    return "menu";
}

}  // namespace track_timer::simulator
