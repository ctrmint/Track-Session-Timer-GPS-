#include "application_screen.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/simulator/summary_fixtures.hpp"

#include <SDL2/SDL.h>
#include <lvgl.h>

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

class MemoryStore final : public track_timer::settings::SettingsStore {
  public:
    track_timer::settings::StoreReadResult read(
        track_timer::settings::SettingsBlob&) noexcept override
    {
        return track_timer::settings::StoreReadResult::missing;
    }

    bool write_atomic(const track_timer::settings::SettingsBlob&) noexcept override
    {
        return true;
    }
};

class CapturingDisplay final : public track_timer::board::DisplayOutput {
  public:
    void apply(const track_timer::board::DisplayCommand& value) noexcept override
    {
        command = value;
        ++apply_count;
    }

    track_timer::board::DisplayCommand command{};
    std::uint32_t apply_count{0};
};

void click(lv_obj_t* object)
{
    assert(object != nullptr);
    assert(lv_obj_send_event(object, LV_EVENT_CLICKED, nullptr) == LV_RESULT_OK);
}

}  // namespace

int main()
{
    using namespace track_timer;

    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    lv_init();
    auto* display = lv_sdl_window_create(600, 450);
    assert(display != nullptr);

    MemoryStore store{};
    settings::SettingsManager settings_manager{store};
    (void)settings_manager.load();
    const auto tracks = simulator::make_track_fixture(simulator::TrackFixtureId::suggested);
    const auto track_match = track::match_track_geofences(tracks.catalog(), tracks.request);
    simulator::SummaryFixtureProvider summaries{simulator::SummaryFixtureId::complete};
    CapturingDisplay display_output{};
    simulator::ApplicationScreen screen{lv_screen_active(), settings_manager, tracks.catalog(),
                                        track_match, &summaries, &display_output};
    ui::ReadySnapshot snapshot{};
    std::strcpy(snapshot.selected_track.data(), "Synthetic Test Loop");
    snapshot.gnss_health = domain::GnssHealth::searching;
    snapshot.storage = ui::Readiness::ready;
    snapshot.imu = ui::Readiness::ready;
    snapshot.logging_available = true;
    diagnostics::DiagnosticsSnapshot diagnostics_snapshot{};
    diagnostics_snapshot.overall = diagnostics::OverallState::normal;
    std::strcpy(diagnostics_snapshot.firmware_version.data(), "application-test");
    diagnostics_snapshot.internal_ram = {diagnostics::SubsystemState::ready, 1'024, 2'048};
    diagnostics_snapshot.psram = {diagnostics::SubsystemState::not_simulated, 0, 0};
    diagnostics_snapshot.backend = diagnostics::BackendKind::simulator;
    diagnostics_snapshot.gnss = diagnostics::SubsystemState::ready;
    diagnostics_snapshot.storage = diagnostics::SubsystemState::ready;
    diagnostics_snapshot.imu = diagnostics::SubsystemState::ready;
    diagnostics_snapshot.rtc = diagnostics::SubsystemState::ready;
    diagnostics_snapshot.touch = diagnostics::SubsystemState::ready;
    diagnostics_snapshot.display = diagnostics::SubsystemState::ready;
    domain::UiSnapshot device_snapshot{};
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 0);
    lv_obj_update_layout(lv_screen_active());

    assert(screen.destination() == ui::Destination::ready);
    assert(std::strcmp(lv_label_get_text(screen.ready_screen().track_label_object()),
                       "Synthetic Test Loop") == 0);
    auto dim_settings = settings_manager.current();
    dim_settings.auto_dim_enabled = true;
    dim_settings.day_brightness_percent = 100;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 0, {},
                  &dim_settings);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot,
                  ui::kReadyAutoDimDelayMs, {}, &dim_settings);
    assert(display_output.command.dimmed);
    assert(display_output.command.brightness_percent == ui::kDimmedBrightnessPercent);
    auto* wake_target = screen.ready_screen().button_for(ui::NavigationAction::start_session);
    assert(lv_obj_send_event(wake_target, LV_EVENT_PRESSED, nullptr) == LV_RESULT_OK);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot,
                  ui::kReadyAutoDimDelayMs + 1, {}, &dim_settings);
    assert(!display_output.command.dimmed);
    assert(display_output.command.brightness_percent == 100);

    for (const auto action : {ui::NavigationAction::start_session,
                              ui::NavigationAction::open_setup,
                              ui::NavigationAction::open_review,
                              ui::NavigationAction::open_diagnostics}) {
        auto* button = screen.ready_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }

    click(screen.ready_screen().button_for(ui::NavigationAction::open_setup));
    assert(screen.destination() == ui::Destination::setup);
    assert(screen.setup_page() == simulator::SetupPage::menu);
    for (const auto action : {ui::SetupMenuAction::track_selection,
                              ui::SetupMenuAction::device_settings,
                              ui::SetupMenuAction::g_meter,
                              ui::SetupMenuAction::back}) {
        auto* button = screen.setup_menu_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::device_settings));
    assert(screen.setup_page() == simulator::SetupPage::device_settings);
    assert(std::strcmp(lv_label_get_text(screen.settings_screen().field_label_object()),
                       "TRACK SESSION") == 0);
    assert(std::strcmp(lv_label_get_text(screen.settings_screen().value_label_object()),
                       "20 MIN") == 0);
    for (const auto action : {ui::SettingsScreenAction::previous_field,
                              ui::SettingsScreenAction::next_field,
                              ui::SettingsScreenAction::decrement,
                              ui::SettingsScreenAction::increment,
                              ui::SettingsScreenAction::save,
                              ui::SettingsScreenAction::cancel,
                              ui::SettingsScreenAction::restore_defaults}) {
        auto* button = screen.settings_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::increment));
    assert(screen.settings_editor().draft().session_duration_minutes == 25);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::restore_defaults));
    assert(screen.settings_editor().status() == ui::SettingsEditorStatus::confirm_defaults);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::cancel_defaults));
    assert(screen.settings_editor().draft().session_duration_minutes == 25);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::save));
    assert(settings_manager.current().session_duration_minutes == 25);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::cancel));
    assert(screen.destination() == ui::Destination::setup);
    assert(screen.setup_page() == simulator::SetupPage::menu);

    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::device_settings));
    for (int count = 0; count < 3; ++count) {
        click(screen.settings_screen().button_for(ui::SettingsScreenAction::next_field));
    }
    assert(screen.settings_editor().field() == ui::SettingsField::day_brightness);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::decrement));
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 1);
    assert(screen.display_policy().snapshot().settings_preview);
    assert(display_output.command.brightness_percent == 75);
    assert(!lv_obj_has_flag(screen.brightness_overlay_object(), LV_OBJ_FLAG_HIDDEN));
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::cancel));
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 2);
    assert(!screen.display_policy().snapshot().settings_preview);
    assert(display_output.command.brightness_percent == 100);
    assert(lv_obj_has_flag(screen.brightness_overlay_object(), LV_OBJ_FLAG_HIDDEN));

    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::device_settings));
    for (int count = 0; count < 3; ++count) {
        click(screen.settings_screen().button_for(ui::SettingsScreenAction::next_field));
    }
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::decrement));
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::save));
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 3);
    assert(settings_manager.current().day_brightness_percent == 75);
    assert(!screen.display_policy().snapshot().settings_preview);
    assert(display_output.command.brightness_percent == 75);
    click(screen.settings_screen().button_for(ui::SettingsScreenAction::cancel));

    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::track_selection));
    assert(screen.setup_page() == simulator::SetupPage::track_selection);
    assert(std::strcmp(lv_label_get_text(screen.track_selection_screen().track_name_object()),
                       "Synthetic Test Loop") == 0);
    for (const auto action : {ui::TrackSelectionAction::previous,
                              ui::TrackSelectionAction::select,
                              ui::TrackSelectionAction::next,
                              ui::TrackSelectionAction::timer_only,
                              ui::TrackSelectionAction::capture_information,
                              ui::TrackSelectionAction::back}) {
        auto* button = screen.track_selection_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.track_selection_screen().button_for(ui::TrackSelectionAction::select));
    assert(std::strcmp(settings_manager.current().selected_track_id.data(),
                       "synthetic_test_loop") == 0);
    click(screen.track_selection_screen().button_for(
        ui::TrackSelectionAction::capture_information));
    assert(screen.track_selection().status() ==
           ui::TrackSelectionStatus::capture_information);
    click(screen.track_selection_screen().button_for(ui::TrackSelectionAction::back));
    assert(screen.setup_page() == simulator::SetupPage::menu);

    ui::ImuMeterInput imu{};
    imu.sample.acceleration_x_mps2 = ui::kStandardGravityMps2 * 0.5F;
    imu.sample.acceleration_y_mps2 = ui::kStandardGravityMps2 * 0.8F;
    imu.sample.valid = true;
    imu.sample_available = true;
    imu.x_axis_valid = true;
    imu.y_axis_valid = true;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 4, {},
                  nullptr, imu);
    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::g_meter));
    assert(screen.setup_page() == simulator::SetupPage::g_meter);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().status_object()),
                       "IMU RECOVERED") == 0);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().direction_object(0)),
                       "ACCEL (+Y)") == 0);
    assert(!lv_obj_has_flag(screen.g_meter_screen().trail_object(0), LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(screen.g_meter_screen().peak_marker_object(), LV_OBJ_FLAG_HIDDEN));
    for (const auto action : {ui::GmeterAction::reset, ui::GmeterAction::back}) {
        auto* button = screen.g_meter_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.g_meter_screen().button_for(ui::GmeterAction::reset));
    assert(screen.g_meter().snapshot().trail_count == 0);

    auto rotated_settings = settings_manager.current();
    rotated_settings.orientation = settings::OrientationMode::fixed_90;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 5, {},
                  &rotated_settings, imu);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().direction_object(0)),
                       "ACCEL (+X)") == 0);
    rotated_settings.orientation = settings::OrientationMode::fixed_180;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 6, {},
                  &rotated_settings, imu);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().direction_object(0)),
                       "ACCEL (-Y)") == 0);
    rotated_settings.orientation = settings::OrientationMode::fixed_270;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 7, {},
                  &rotated_settings, imu);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().direction_object(0)),
                       "ACCEL (-X)") == 0);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 8, {},
                  nullptr, imu);
    assert(std::strcmp(lv_label_get_text(screen.g_meter_screen().direction_object(0)),
                       "ACCEL (+Y)") == 0);
    click(screen.g_meter_screen().button_for(ui::GmeterAction::back));
    assert(screen.setup_page() == simulator::SetupPage::menu);
    click(screen.setup_menu_screen().button_for(ui::SetupMenuAction::back));
    assert(screen.destination() == ui::Destination::ready);

    click(screen.ready_screen().button_for(ui::NavigationAction::open_review));
    assert(screen.destination() == ui::Destination::review);
    assert(screen.session_review().view_model().status == ui::SessionReviewStatus::ready);
    assert(std::strcmp(lv_label_get_text(screen.session_review_screen().lap_object(0)),
                       "1:04.210") == 0);
    for (const auto action : {ui::SessionReviewAction::newer_session,
                              ui::SessionReviewAction::older_session,
                              ui::SessionReviewAction::previous_page,
                              ui::SessionReviewAction::next_page,
                              ui::SessionReviewAction::return_to_rest,
                              ui::SessionReviewAction::return_to_ready}) {
        auto* button = screen.session_review_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::next_page));
    assert(screen.session_review().view_model().lap_offset == 4);
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::previous_page));
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::older_session));
    assert(screen.session_review().view_model().history_index == 1);
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::newer_session));
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::return_to_ready));
    assert(screen.destination() == ui::Destination::ready);
    assert(screen.consume_ready_request());

    click(screen.ready_screen().button_for(ui::NavigationAction::open_review));
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::return_to_rest));
    assert(screen.destination() == ui::Destination::ready);
    assert(screen.consume_rest_request());
    click(screen.ready_screen().button_for(ui::NavigationAction::open_diagnostics));
    assert(screen.destination() == ui::Destination::diagnostics);
    assert(screen.diagnostics().view_model().current_page == ui::DiagnosticsPage::system);
    assert(std::strcmp(lv_label_get_text(screen.diagnostics_screen().row_value_object(0)),
                       "application-test") == 0);
    for (const auto action : {ui::DiagnosticsAction::previous_page,
                              ui::DiagnosticsAction::next_page,
                              ui::DiagnosticsAction::back}) {
        auto* button = screen.diagnostics_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }
    click(screen.diagnostics_screen().button_for(ui::DiagnosticsAction::next_page));
    assert(screen.diagnostics().view_model().current_page == ui::DiagnosticsPage::gnss);
    click(screen.diagnostics_screen().button_for(ui::DiagnosticsAction::next_page));
    click(screen.diagnostics_screen().button_for(ui::DiagnosticsAction::next_page));
    assert(screen.diagnostics().view_model().current_page == ui::DiagnosticsPage::peripherals);
    assert(std::strstr(lv_label_get_text(
                           screen.diagnostics_screen().row_value_object(7)),
                       "75% / 0 DEG") != nullptr);
    click(screen.diagnostics_screen().button_for(ui::DiagnosticsAction::back));
    assert(screen.destination() == ui::Destination::ready);

    click(screen.ready_screen().button_for(ui::NavigationAction::start_session));
    assert(screen.destination() == ui::Destination::active);
    assert(screen.consume_start_request());
    device_snapshot.session_active = true;
    device_snapshot.session_remaining_ms = 15 * 60'000;
    device_snapshot.current_lap_ms = 54'000;
    device_snapshot.previous_lap_ms = 1 * 60'000 + 41'000;
    device_snapshot.best_lap_ms = 1 * 60'000 + 40'500;
    device_snapshot.lap_index = 7;
    device_snapshot.gnss_health = domain::GnssHealth::good;
    device_snapshot.logging_available = true;
    screen.synchronize_session(true);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 1'000);
    for (const auto action : {simulator::DeviceScreenAction::press_stop,
                              simulator::DeviceScreenAction::cancel_stop,
                              simulator::DeviceScreenAction::confirm_stop}) {
        auto* button = screen.device_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }

    auto* stop_button = screen.device_screen().button_for(
        simulator::DeviceScreenAction::press_stop);
    assert(lv_obj_send_event(stop_button, LV_EVENT_CLICKED, nullptr) == LV_RESULT_OK);
    assert(lv_obj_send_event(stop_button, LV_EVENT_GESTURE, nullptr) == LV_RESULT_OK);
    assert(screen.active_session().view_model().stop.state == ui::StopControlState::idle);
    assert(!screen.consume_stop_request());

    assert(lv_obj_send_event(stop_button, LV_EVENT_PRESSED, nullptr) == LV_RESULT_OK);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 2'499);
    assert(lv_obj_send_event(screen.device_screen().button_for(
                                 simulator::DeviceScreenAction::release_stop),
                             LV_EVENT_RELEASED, nullptr) == LV_RESULT_OK);
    assert(screen.active_session().view_model().stop.state == ui::StopControlState::idle);
    assert(!screen.consume_stop_request());

    assert(lv_obj_send_event(screen.device_screen().button_for(
                                 simulator::DeviceScreenAction::press_stop),
                             LV_EVENT_PRESSED, nullptr) == LV_RESULT_OK);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 4'000);
    assert(lv_obj_send_event(screen.device_screen().button_for(
                                 simulator::DeviceScreenAction::release_stop),
                             LV_EVENT_RELEASED, nullptr) == LV_RESULT_OK);
    assert(screen.active_session().view_model().stop.state ==
           ui::StopControlState::confirming);
    click(screen.device_screen().button_for(simulator::DeviceScreenAction::cancel_stop));
    assert(screen.active_session().view_model().stop.state == ui::StopControlState::idle);

    assert(lv_obj_send_event(screen.device_screen().button_for(
                                 simulator::DeviceScreenAction::press_stop),
                             LV_EVENT_PRESSED, nullptr) == LV_RESULT_OK);
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 5'500);
    assert(lv_obj_send_event(screen.device_screen().button_for(
                                 simulator::DeviceScreenAction::release_stop),
                             LV_EVENT_RELEASED, nullptr) == LV_RESULT_OK);
    click(screen.device_screen().button_for(simulator::DeviceScreenAction::confirm_stop));
    assert(screen.consume_stop_request());

    device_snapshot.lap_index = 8;
    device_snapshot.previous_lap_ms = 1 * 60'000 + 39'750;
    device_snapshot.best_lap_ms = device_snapshot.previous_lap_ms;
    screen.update(ui::present_ready(snapshot), device_snapshot, diagnostics_snapshot, 5'750);
    assert(screen.active_session().view_model().feedback.kind ==
           ui::LapFeedbackKind::faster);
    lv_obj_update_layout(lv_screen_active());
    assert(!lv_obj_has_flag(screen.device_screen().feedback_panel_object(),
                            LV_OBJ_FLAG_HIDDEN));
    const auto* feedback_panel = screen.device_screen().feedback_panel_object();
    const auto* session_panel = screen.device_screen().session_panel_object();
    assert(lv_obj_get_y(feedback_panel) + lv_obj_get_height(feedback_panel) <=
           lv_obj_get_y(session_panel));
    assert(std::strcmp(lv_label_get_text(
                           screen.device_screen().feedback_comparison_object()),
                       "-0.750 FASTER") == 0);
    assert(!screen.navigate(ui::NavigationAction::open_setup).accepted);
    assert(!screen.navigate(ui::NavigationAction::open_diagnostics).accepted);
    assert(screen.destination() == ui::Destination::active);
    assert(screen.navigate(ui::NavigationAction::session_ended).current ==
           ui::Destination::ready);

    session::SessionController lifecycle{{60'000, 30'000}};
    assert(lifecycle.start(0) == session::TransitionResult::accepted);
    assert(lifecycle.advance(65'000) == session::TransitionResult::accepted);
    auto overtime_snapshot = ui::apply_session_timing(device_snapshot,
                                                       lifecycle.snapshot());
    screen.synchronize_workflow(lifecycle.snapshot(), 65'000);
    screen.update(ui::present_ready(snapshot), overtime_snapshot, diagnostics_snapshot,
                  65'000);
    assert(screen.destination() == ui::Destination::active);
    assert(std::strcmp(lv_label_get_text(screen.device_screen().session_status_object()),
                       "OVERTIME") == 0);
    assert(std::strcmp(ui::present(overtime_snapshot).session_remaining.data(),
                       "+00:05") == 0);

    assert(lifecycle.request_stop(65'000) == session::TransitionResult::accepted);
    assert(lifecycle.confirm_stop(65'001) == session::TransitionResult::accepted);
    screen.synchronize_workflow(lifecycle.snapshot(), 65'001);
    assert(screen.destination() == ui::Destination::review);
    click(screen.session_review_screen().button_for(ui::SessionReviewAction::return_to_rest));
    assert(screen.consume_rest_request());
    assert(lifecycle.complete_review(65'002) == session::TransitionResult::accepted);
    screen.synchronize_workflow(lifecycle.snapshot(), 65'002);
    assert(screen.destination() == ui::Destination::rest);
    assert(std::strcmp(lv_label_get_text(screen.rest_screen().title_object()),
                       "REST / RECOVERY") == 0);
    assert(std::strstr(lv_label_get_text(screen.rest_screen().completion_object()),
                       "DRIVER STOP") != nullptr);
    assert(std::strcmp(screen.rest_session().view_model().remaining.data(), "00:30") == 0);
    auto* remaining = screen.rest_screen().remaining_object();
    assert(lv_obj_get_child_count(remaining) == 5);
    assert(std::strcmp(lv_label_get_text(lv_obj_get_child(remaining, 0)), "0") == 0);
    assert(std::strcmp(lv_label_get_text(lv_obj_get_child(remaining, 2)), ":") == 0);
    assert(std::strcmp(lv_label_get_text(lv_obj_get_child(remaining, 4)), "0") == 0);
    for (const auto action : {simulator::RestScreenAction::press_skip,
                              simulator::RestScreenAction::cancel_skip,
                              simulator::RestScreenAction::confirm_skip}) {
        auto* button = screen.rest_screen().button_for(action);
        assert(button != nullptr);
        assert(lv_obj_get_width(button) >= 56);
        assert(lv_obj_get_height(button) >= 56);
    }

    auto* skip_button = screen.rest_screen().button_for(
        simulator::RestScreenAction::press_skip);
    assert(lv_obj_send_event(skip_button, LV_EVENT_CLICKED, nullptr) == LV_RESULT_OK);
    assert(lv_obj_send_event(skip_button, LV_EVENT_GESTURE, nullptr) == LV_RESULT_OK);
    assert(!screen.consume_skip_rest_request());
    assert(lv_obj_send_event(skip_button, LV_EVENT_PRESSED, nullptr) == LV_RESULT_OK);
    screen.synchronize_workflow(lifecycle.snapshot(), 66'502);
    assert(lv_obj_send_event(screen.rest_screen().button_for(
                                 simulator::RestScreenAction::release_skip),
                             LV_EVENT_RELEASED, nullptr) == LV_RESULT_OK);
    click(screen.rest_screen().button_for(simulator::RestScreenAction::confirm_skip));
    assert(screen.consume_skip_rest_request());
    assert(lifecycle.request_stop(66'502) == session::TransitionResult::accepted);
    assert(lifecycle.confirm_stop(66'503) == session::TransitionResult::accepted);
    screen.synchronize_workflow(lifecycle.snapshot(), 66'503);
    assert(screen.destination() == ui::Destination::ready);

    lv_display_delete(display);
    lv_sdl_quit();
    lv_deinit();
    std::cout << "Ready dashboard controls and application destinations passed\n";
    return 0;
}
