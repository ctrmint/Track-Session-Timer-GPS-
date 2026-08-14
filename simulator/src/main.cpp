#include "application_screen.hpp"

#include "track_timer/settings/settings.hpp"
#include "track_timer/session/controller.hpp"
#include "track_timer/simulator/file_settings_store.hpp"
#include "track_timer/simulator/file_track_definition_store.hpp"
#include "track_timer/simulator/diagnostics_fixtures.hpp"
#include "track_timer/simulator/display_fixtures.hpp"
#include "track_timer/simulator/imu_fixtures.hpp"
#include "track_timer/simulator/scenario.hpp"
#include "track_timer/simulator/summary_fixtures.hpp"
#include "track_timer/simulator/track_catalog_loader.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/track/matching.hpp"
#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/presenter.hpp"

#include <SDL2/SDL.h>
#include <lvgl.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr std::int32_t kDisplayWidth = 600;
constexpr std::int32_t kDisplayHeight = 450;

enum class WorkflowFixture : std::uint8_t {
    live,
    overtime,
    completion,
    rest,
};

enum class TrackdayFixture : std::uint8_t {
    saved,
    enabled,
    disabled,
};

const char* trackday_fixture_name(const TrackdayFixture fixture) noexcept
{
    switch (fixture) {
    case TrackdayFixture::saved:
        return "saved";
    case TrackdayFixture::enabled:
        return "enabled";
    case TrackdayFixture::disabled:
        return "disabled";
    }
    return "saved";
}

bool parse_trackday_fixture(const std::string& name, TrackdayFixture& fixture) noexcept
{
    for (const auto candidate : {TrackdayFixture::saved, TrackdayFixture::enabled,
                                 TrackdayFixture::disabled}) {
        if (name == trackday_fixture_name(candidate)) {
            fixture = candidate;
            return true;
        }
    }
    return false;
}

const char* workflow_fixture_name(const WorkflowFixture fixture) noexcept
{
    switch (fixture) {
    case WorkflowFixture::live:
        return "live";
    case WorkflowFixture::overtime:
        return "overtime";
    case WorkflowFixture::completion:
        return "completion";
    case WorkflowFixture::rest:
        return "rest";
    }
    return "live";
}

bool parse_workflow_fixture(const std::string& name, WorkflowFixture& fixture) noexcept
{
    for (const auto candidate : {WorkflowFixture::live, WorkflowFixture::overtime,
                                 WorkflowFixture::completion, WorkflowFixture::rest}) {
        if (name == workflow_fixture_name(candidate)) {
            fixture = candidate;
            return true;
        }
    }
    return false;
}

struct Options {
    track_timer::simulator::ScenarioId scenario{track_timer::simulator::ScenarioId::ready};
    track_timer::simulator::GnssReplayRate gnss_rate{
        track_timer::simulator::GnssReplayRate::hz25};
    bool headless{false};
    std::size_t frames{25};
    std::int64_t frame_ms{40};
    std::string snapshot_path{};
    std::string gnss_fixture_path{};
    std::string initial_screen{"ready"};
    track_timer::simulator::TrackFixtureId track_fixture{
        track_timer::simulator::TrackFixtureId::suggested};
    bool track_fixture_explicit{false};
    track_timer::simulator::SummaryFixtureId summary_fixture{
        track_timer::simulator::SummaryFixtureId::complete};
    track_timer::simulator::DiagnosticsFixtureId diagnostics_fixture{
        track_timer::simulator::DiagnosticsFixtureId::normal};
    track_timer::simulator::DisplayFixtureId display_fixture{
        track_timer::simulator::DisplayFixtureId::live};
    track_timer::simulator::ImuFixtureId imu_fixture{
        track_timer::simulator::ImuFixtureId::normal};
    WorkflowFixture workflow_fixture{WorkflowFixture::live};
    TrackdayFixture trackday_fixture{TrackdayFixture::saved};
};

std::string require_value(const int argc, char** argv, int& index)
{
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string("missing value for ") + argv[index]);
    }
    return argv[++index];
}

std::size_t parse_positive_size(const std::string& value, const char* option)
{
    const auto parsed = std::stoull(value);
    if (parsed == 0) {
        throw std::invalid_argument(std::string(option) + " must be greater than zero");
    }
    return static_cast<std::size_t>(parsed);
}

Options parse_options(const int argc, char** argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        if (argument == "--headless") {
            options.headless = true;
        }
        else if (argument == "--scenario") {
            const auto name = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_scenario(name, options.scenario)) {
                throw std::invalid_argument("unknown scenario: " + name);
            }
        }
        else if (argument == "--frames") {
            options.frames = parse_positive_size(require_value(argc, argv, index), "--frames");
        }
        else if (argument == "--frame-ms") {
            options.frame_ms = static_cast<std::int64_t>(
                parse_positive_size(require_value(argc, argv, index), "--frame-ms"));
        }
        else if (argument == "--gnss-rate") {
            const auto value = require_value(argc, argv, index);
            if (value == "20") {
                options.gnss_rate = track_timer::simulator::GnssReplayRate::hz20;
            }
            else if (value == "25") {
                options.gnss_rate = track_timer::simulator::GnssReplayRate::hz25;
            }
            else {
                throw std::invalid_argument("--gnss-rate must be 20 or 25");
            }
        }
        else if (argument == "--gnss-fixture") {
            options.gnss_fixture_path = require_value(argc, argv, index);
        }
        else if (argument == "--snapshot") {
            options.snapshot_path = require_value(argc, argv, index);
        }
        else if (argument == "--screen") {
            options.initial_screen = require_value(argc, argv, index);
            if (options.initial_screen != "ready" && options.initial_screen != "setup" &&
                options.initial_screen != "settings" && options.initial_screen != "tracks" &&
                options.initial_screen != "gate-capture" &&
                options.initial_screen != "g-meter" && options.initial_screen != "review" &&
                options.initial_screen != "diagnostics") {
                throw std::invalid_argument(
                    "--screen must be ready, setup, settings, tracks, gate-capture, g-meter, review, or diagnostics");
            }
        }
        else if (argument == "--imu-state") {
            const auto value = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_imu_fixture(value, options.imu_fixture)) {
                throw std::invalid_argument(
                    "--imu-state must be normal, calibration, failure, partial, or recovery");
            }
        }
        else if (argument == "--workflow-state") {
            const auto value = require_value(argc, argv, index);
            if (!parse_workflow_fixture(value, options.workflow_fixture)) {
                throw std::invalid_argument(
                    "--workflow-state must be live, overtime, completion, or rest");
            }
        }
        else if (argument == "--trackday-mode") {
            const auto value = require_value(argc, argv, index);
            if (!parse_trackday_fixture(value, options.trackday_fixture)) {
                throw std::invalid_argument(
                    "--trackday-mode must be saved, enabled, or disabled");
            }
        }
        else if (argument == "--review-state") {
            const auto value = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_summary_fixture(value,
                                                                options.summary_fixture)) {
                throw std::invalid_argument(
                    "--review-state must be complete, partial, empty, missing, corrupt, or unsupported");
            }
        }
        else if (argument == "--track-state") {
            const auto value = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_track_fixture(value, options.track_fixture)) {
                throw std::invalid_argument(
                    "--track-state must be selected, missing, invalid, ambiguous, suggested, none, or unavailable");
            }
            options.track_fixture_explicit = true;
        }
        else if (argument == "--diagnostics-state") {
            const auto value = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_diagnostics_fixture(
                    value, options.diagnostics_fixture)) {
                throw std::invalid_argument(
                    "--diagnostics-state must be normal, degraded, missing, or recovery");
            }
        }
        else if (argument == "--display-state") {
            const auto value = require_value(argc, argv, index);
            if (!track_timer::simulator::parse_display_fixture(value,
                                                                options.display_fixture)) {
                throw std::invalid_argument(
                    "--display-state must be live, day, night, dimmed, or rotated");
            }
        }
        else if (argument == "--help") {
            std::cout << "Usage: track_timer_simulator [--scenario NAME] [--headless] "
                         "[--frames COUNT] [--frame-ms MS] [--gnss-rate 20|25] "
                         "[--gnss-fixture FILE] [--screen ready|setup|settings|tracks|gate-capture|g-meter|review|diagnostics] "
                         "[--track-state STATE] [--review-state STATE] "
                         "[--diagnostics-state STATE] [--display-state STATE] [--imu-state STATE] [--workflow-state STATE] [--trackday-mode saved|enabled|disabled] "
                         "[--snapshot FILE]\n"
                         "Scenarios: ready, active, gnss-loss, storage-failure, "
                         "lap-faster, lap-slower, lap-unavailable-best\n"
                         "Use pointer/touch controls or keyboard focus and Enter to navigate.\n";
            std::exit(0);
        }
        else {
            throw std::invalid_argument("unknown option: " + argument);
        }
    }
    return options;
}

bool write_snapshot(const std::string& path)
{
    auto* snapshot = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if (snapshot == nullptr || snapshot->header.w != kDisplayWidth ||
        snapshot->header.h != kDisplayHeight) {
        if (snapshot != nullptr) {
            lv_draw_buf_destroy(snapshot);
        }
        return false;
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        lv_draw_buf_destroy(snapshot);
        return false;
    }

    output << "P6\n" << snapshot->header.w << ' ' << snapshot->header.h << "\n255\n";
    for (std::uint32_t y = 0; y < snapshot->header.h; ++y) {
        const auto* row = snapshot->data + y * snapshot->header.stride;
        for (std::uint32_t x = 0; x < snapshot->header.w; ++x) {
            const auto* pixel = reinterpret_cast<const lv_color32_t*>(row + x * sizeof(lv_color32_t));
            output.put(static_cast<char>(pixel->red));
            output.put(static_cast<char>(pixel->green));
            output.put(static_cast<char>(pixel->blue));
        }
    }
    const bool succeeded = static_cast<bool>(output);
    lv_draw_buf_destroy(snapshot);
    return succeeded;
}

struct ApplicationContext {
    track_timer::simulator::ScenarioPlayer player;
    track_timer::simulator::ApplicationScreen* screen;
    track_timer::settings::SettingsManager* settings;
    const track_timer::simulator::TrackFixture* track_fixture;
    track_timer::simulator::DiagnosticsFixtureId diagnostics_fixture;
    track_timer::simulator::DisplayFixture display_fixture;
    track_timer::simulator::ImuFixtureId imu_fixture;
    WorkflowFixture workflow_fixture;
    TrackdayFixture trackday_fixture;
    track_timer::settings::DeviceSettings active_settings_fixture{};
    track_timer::session::SessionController lifecycle{{60'000, 30'000}};
    std::int64_t workflow_time_offset_ms{0};
    bool workflow_active{false};
    lv_display_t* display;
    track_timer::ui::RenderProfiler profiler{};

    ApplicationContext(const track_timer::simulator::ScenarioId scenario,
                       track_timer::simulator::GnssFixture fixture,
                       const track_timer::simulator::GnssReplayRate rate,
                       track_timer::simulator::ApplicationScreen* application_screen,
                       track_timer::settings::SettingsManager* settings_manager,
                       const track_timer::simulator::TrackFixture* fixture_tracks,
                       const track_timer::simulator::DiagnosticsFixtureId fixture_diagnostics,
                       const track_timer::simulator::DisplayFixture& fixture_display,
                       const track_timer::simulator::ImuFixtureId fixture_imu,
                       const WorkflowFixture fixture_workflow,
                       const TrackdayFixture fixture_trackday,
                       lv_display_t* target_display)
        : player(scenario, std::move(fixture), rate), screen(application_screen),
          settings(settings_manager), track_fixture(fixture_tracks),
          diagnostics_fixture(fixture_diagnostics), display_fixture(fixture_display),
          imu_fixture(fixture_imu), workflow_fixture(fixture_workflow),
          trackday_fixture(fixture_trackday),
          active_settings_fixture(settings_manager->current()),
          display(target_display)
    {
        if (trackday_fixture != TrackdayFixture::saved) {
            active_settings_fixture.trackday_mode_enabled =
                trackday_fixture == TrackdayFixture::enabled;
            if (active_settings_fixture.trackday_mode_enabled &&
                active_settings_fixture.average_lap_seconds == 0) {
                active_settings_fixture.average_lap_seconds = 100;
            }
        }
        switch (workflow_fixture) {
        case WorkflowFixture::live:
            return;
        case WorkflowFixture::overtime:
            (void)lifecycle.start(0);
            workflow_time_offset_ms = 65'000;
            workflow_active = true;
            break;
        case WorkflowFixture::completion:
            (void)lifecycle.start(0);
            (void)lifecycle.request_stop(65'000);
            (void)lifecycle.confirm_stop(65'001);
            workflow_time_offset_ms = 65'001;
            workflow_active = true;
            player.stop_session();
            break;
        case WorkflowFixture::rest:
            (void)lifecycle.start(0);
            (void)lifecycle.request_stop(65'000);
            (void)lifecycle.confirm_stop(65'001);
            (void)lifecycle.complete_review(65'002);
            workflow_time_offset_ms = 65'002;
            workflow_active = true;
            player.stop_session();
            break;
        }
    }
};

void update_screen(ApplicationContext& context)
{
    const auto workflow_now = context.player.elapsed_ms() + context.workflow_time_offset_ms;
    if (context.screen->consume_start_request()) {
        context.player.reset(track_timer::simulator::ScenarioId::active);
        context.workflow_time_offset_ms = workflow_now;
        context.lifecycle = track_timer::session::SessionController{{
            static_cast<std::int64_t>(context.settings->current().session_duration_minutes) *
                60'000,
            static_cast<std::int64_t>(context.settings->current().rest_duration_minutes) *
                60'000}};
        (void)context.lifecycle.start(workflow_now);
        context.workflow_active = true;
    }
    if (context.screen->consume_stop_request()) {
        if (context.workflow_active) {
            (void)context.lifecycle.request_stop(workflow_now);
            (void)context.lifecycle.confirm_stop(workflow_now);
        }
        context.player.stop_session();
    }

    const auto rest_requested = context.screen->consume_rest_request();
    const auto ready_requested = context.screen->consume_ready_request();
    if (context.workflow_active && (rest_requested || ready_requested) &&
        context.lifecycle.snapshot().state == track_timer::session::SessionState::review) {
        (void)context.lifecycle.complete_review(workflow_now);
        if (ready_requested &&
            context.lifecycle.snapshot().state == track_timer::session::SessionState::rest) {
            (void)context.lifecycle.request_stop(workflow_now);
            (void)context.lifecycle.confirm_stop(workflow_now);
        }
    }
    if (context.workflow_active && context.screen->consume_skip_rest_request() &&
        context.lifecycle.snapshot().state == track_timer::session::SessionState::rest) {
        (void)context.lifecycle.request_stop(workflow_now);
        (void)context.lifecycle.confirm_stop(workflow_now);
    }
    if (context.workflow_active) {
        (void)context.lifecycle.advance(workflow_now);
    }

    auto active_snapshot = context.player.snapshot();
    if (context.workflow_active) {
        active_snapshot = track_timer::ui::apply_session_timing(
            active_snapshot, context.lifecycle.snapshot());
        context.screen->synchronize_workflow(context.lifecycle.snapshot(),
                                             static_cast<std::uint64_t>(workflow_now));
    }
    else {
        context.screen->synchronize_session(active_snapshot.session_active);
    }

    auto match_request = context.track_fixture->request;
    match_request.selected_track_id = context.settings->current().selected_track_id.data();
    const auto track_match = track_timer::track::match_track_geofences(
        context.track_fixture->catalog(), match_request);
    context.screen->update_track_match(track_match);

    track_timer::ui::ReadySnapshot ready{};
    if (track_match.state == track_timer::track::TrackMatchState::selected &&
        track_match.selected_index < context.track_fixture->count) {
        std::snprintf(ready.selected_track.data(), ready.selected_track.size(), "%s",
                      context.track_fixture->definitions[track_match.selected_index].name.data());
        ready.track_state = track_timer::ui::ReadyTrackState::selected;
    }
    else {
        switch (track_match.state) {
        case track_timer::track::TrackMatchState::suggested:
            ready.track_state = track_timer::ui::ReadyTrackState::suggested;
            break;
        case track_timer::track::TrackMatchState::ambiguous:
            ready.track_state = track_timer::ui::ReadyTrackState::ambiguous;
            break;
        case track_timer::track::TrackMatchState::selected_track_missing:
            ready.track_state = track_timer::ui::ReadyTrackState::missing;
            break;
        case track_timer::track::TrackMatchState::invalid_catalog:
            ready.track_state = track_timer::ui::ReadyTrackState::invalid;
            break;
        case track_timer::track::TrackMatchState::location_unavailable:
        case track_timer::track::TrackMatchState::no_match:
        case track_timer::track::TrackMatchState::selected:
            ready.track_state = track_timer::ui::ReadyTrackState::none;
            break;
        }
    }
    ready.session_duration_minutes = context.settings->current().session_duration_minutes;
    ready.rest_duration_minutes = context.settings->current().rest_duration_minutes;
    ready.gnss_health = active_snapshot.gnss_health;
    const auto storage_health = context.player.device().storage().status().health;
    ready.storage = storage_health == track_timer::board::StorageHealth::ready
                        ? track_timer::ui::Readiness::ready
                    : storage_health == track_timer::board::StorageHealth::degraded
                        ? track_timer::ui::Readiness::degraded
                        : track_timer::ui::Readiness::unavailable;
    switch (context.imu_fixture) {
    case track_timer::simulator::ImuFixtureId::normal:
        ready.imu = track_timer::ui::Readiness::ready;
        break;
    case track_timer::simulator::ImuFixtureId::calibration:
    case track_timer::simulator::ImuFixtureId::partial:
        ready.imu = track_timer::ui::Readiness::degraded;
        break;
    case track_timer::simulator::ImuFixtureId::failure:
        ready.imu = track_timer::ui::Readiness::unavailable;
        break;
    case track_timer::simulator::ImuFixtureId::recovery:
        ready.imu = context.player.elapsed_ms() < 600
                        ? track_timer::ui::Readiness::unavailable
                        : track_timer::ui::Readiness::ready;
        break;
    }
    ready.logging_available = active_snapshot.logging_available;
    ready.session_active = active_snapshot.session_active;

    auto diagnostics = track_timer::simulator::make_diagnostics_snapshot(
        context.diagnostics_fixture,
        static_cast<std::uint64_t>(context.player.elapsed_ms()), context.player.diagnostics(),
        context.player.device().storage().status(), context.player.logger_metrics(),
        context.profiler.metrics());
    const auto imu = track_timer::simulator::make_imu_fixture_input(
        context.imu_fixture, static_cast<std::uint64_t>(context.player.elapsed_ms()));
    if (context.imu_fixture == track_timer::simulator::ImuFixtureId::failure ||
        (context.imu_fixture == track_timer::simulator::ImuFixtureId::recovery &&
         context.player.elapsed_ms() < 600)) {
        diagnostics.imu = track_timer::diagnostics::SubsystemState::unavailable;
    }
    else if (context.imu_fixture == track_timer::simulator::ImuFixtureId::calibration ||
             context.imu_fixture == track_timer::simulator::ImuFixtureId::partial) {
        diagnostics.imu = track_timer::diagnostics::SubsystemState::degraded;
    }
    else {
        diagnostics.imu = track_timer::diagnostics::SubsystemState::ready;
    }
    track_timer::domain::GnssFix latest_fix{};
    if (context.player.latest_fix(latest_fix)) {
        context.screen->update_capture_fix(
            latest_fix, context.player.device().clock().now_us());
    }
    context.screen->update(
        track_timer::ui::present_ready(ready), active_snapshot, diagnostics,
        static_cast<std::uint64_t>(context.player.elapsed_ms()), context.display_fixture.input,
        context.display_fixture.override_settings ? &context.display_fixture.settings : nullptr,
        imu, context.trackday_fixture == TrackdayFixture::saved
                 ? nullptr
                 : &context.active_settings_fixture);
}

void render_screen(ApplicationContext& context)
{
    const auto started = std::chrono::steady_clock::now();
    update_screen(context);
    lv_timer_handler();
    lv_refr_now(context.display);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    const auto used_bytes = memory.total_size >= memory.free_size
                                ? memory.total_size - memory.free_size
                                : 0;
    const auto bounded_elapsed = elapsed <= 0
                                     ? 0U
                                     : elapsed > static_cast<std::int64_t>(UINT32_MAX)
                                           ? UINT32_MAX
                                           : static_cast<std::uint32_t>(elapsed);
    context.profiler.record(bounded_elapsed, used_bytes);
}

int run(const Options& options)
{
    if (options.headless && std::getenv("SDL_VIDEODRIVER") == nullptr) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    }

    lv_init();
    auto* display = lv_sdl_window_create(kDisplayWidth, kDisplayHeight);
    if (display == nullptr) {
        std::cerr << "Failed to create the 600 x 450 SDL display\n";
        lv_deinit();
        return 1;
    }
    lv_sdl_window_set_title(display, "Track Session Timer — device simulator");
    lv_sdl_window_set_resizeable(display, false);

    auto* mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, display);
    auto* keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, display);
    auto* input_group = lv_group_create();
    lv_indev_set_group(keyboard, input_group);

    auto fixture = track_timer::simulator::make_synthetic_gnss_fixture();
    if (!options.gnss_fixture_path.empty()) {
        std::string error;
        if (!track_timer::simulator::load_gnss_fixture(options.gnss_fixture_path, fixture, error)) {
            throw std::invalid_argument(error);
        }
    }

    const auto settings_path = std::filesystem::temp_directory_path() /
                               "track-session-timer-simulator" / "settings-v2.bin";
    track_timer::simulator::FileSettingsStore settings_store{settings_path};
    track_timer::settings::SettingsManager settings_manager{settings_store};
    (void)settings_manager.load();
    const auto track_store_path = std::filesystem::temp_directory_path() /
                                  "track-session-timer-simulator" / "tracks";
    track_timer::simulator::FileTrackDefinitionStore track_store{track_store_path};

    auto track_fixture = track_timer::simulator::make_track_fixture(options.track_fixture);
    if (!options.track_fixture_explicit) {
        std::string catalog_error;
        if (!track_timer::simulator::append_track_catalog_directory(
                TRACK_TIMER_UK_TRACK_PACK_DIR, track_fixture, catalog_error)) {
            throw std::invalid_argument("cannot load UK simulator track catalog: " +
                                        catalog_error);
        }
    }
    if (options.track_fixture_explicit) {
        auto fixture_settings = settings_manager.current();
        fixture_settings.selected_track_id.fill('\0');
        if (!track_fixture.request.selected_track_id.empty()) {
            std::snprintf(fixture_settings.selected_track_id.data(),
                          fixture_settings.selected_track_id.size(), "%.*s",
                          static_cast<int>(track_fixture.request.selected_track_id.size()),
                          track_fixture.request.selected_track_id.data());
        }
        (void)settings_manager.apply(fixture_settings, false);
    }
    auto initial_request = track_fixture.request;
    initial_request.selected_track_id = settings_manager.current().selected_track_id.data();
    const auto initial_match = track_timer::track::match_track_geofences(
        track_fixture.catalog(), initial_request);
    track_timer::simulator::SummaryFixtureProvider summary_provider{options.summary_fixture};
    const auto display_fixture = track_timer::simulator::make_display_fixture(
        options.display_fixture, settings_manager.current());

    track_timer::simulator::ApplicationScreen screen{lv_screen_active(), settings_manager,
                                                      track_fixture.catalog(), initial_match,
                                                      &summary_provider, nullptr, &track_store};
    screen.add_controls_to_group(input_group);
    if (options.initial_screen != "ready") {
        if (options.initial_screen == "review") {
            (void)screen.navigate(track_timer::ui::NavigationAction::open_review);
        }
        else if (options.initial_screen == "diagnostics") {
            (void)screen.navigate(track_timer::ui::NavigationAction::open_diagnostics);
        }
        else {
            (void)screen.navigate(track_timer::ui::NavigationAction::open_setup);
            if (options.initial_screen == "settings") {
                screen.open_setup_page(track_timer::simulator::SetupPage::device_settings);
            }
            else if (options.initial_screen == "tracks") {
                screen.open_setup_page(track_timer::simulator::SetupPage::track_selection);
            }
            else if (options.initial_screen == "gate-capture") {
                screen.open_setup_page(track_timer::simulator::SetupPage::track_selection);
                (void)lv_obj_send_event(
                    screen.track_selection_screen().button_for(
                        track_timer::ui::TrackSelectionAction::capture_information),
                    LV_EVENT_CLICKED, nullptr);
            }
            else if (options.initial_screen == "g-meter") {
                screen.open_setup_page(track_timer::simulator::SetupPage::g_meter);
            }
        }
    }
    ApplicationContext context{options.scenario, std::move(fixture), options.gnss_rate, &screen,
                               &settings_manager, &track_fixture,
                               options.diagnostics_fixture, display_fixture,
                               options.imu_fixture, options.workflow_fixture,
                               options.trackday_fixture, display};
    render_screen(context);

    if (options.headless) {
        for (std::size_t frame = 0; frame < options.frames; ++frame) {
            context.player.advance(options.frame_ms);
            render_screen(context);
        }
    }
    else {
        auto previous_tick = SDL_GetTicks64();
        while (lv_display_get_next(nullptr) != nullptr) {
            const auto current_tick = SDL_GetTicks64();
            if (current_tick - previous_tick >= static_cast<std::uint64_t>(options.frame_ms)) {
                context.player.advance(options.frame_ms);
                render_screen(context);
                previous_tick = current_tick;
            }
            else {
                lv_timer_handler();
            }
            SDL_Delay(5);
        }
    }

    if (!options.snapshot_path.empty() && !write_snapshot(options.snapshot_path)) {
        std::cerr << "Failed to write snapshot: " << options.snapshot_path << '\n';
        lv_display_delete(display);
        lv_sdl_quit();
        lv_deinit();
        return 1;
    }

    const auto& model = screen.active_session().view_model().timing;
    const auto diagnostics = context.player.diagnostics();
    const auto& gnss = context.player.device().gnss();
    const auto& storage = context.player.device().storage();
    const auto logger_metrics = context.player.logger_metrics();
    const auto render_metrics = context.profiler.metrics();
    const auto diagnostics_snapshot = track_timer::simulator::make_diagnostics_snapshot(
        options.diagnostics_fixture,
        static_cast<std::uint64_t>(context.player.elapsed_ms()), diagnostics,
        storage.status(), logger_metrics, render_metrics);
    auto final_track_request = track_fixture.request;
    final_track_request.selected_track_id = settings_manager.current().selected_track_id.data();
    const auto final_track_match = track_timer::track::match_track_geofences(
        track_fixture.catalog(), final_track_request);
    std::cout << "scenario=" << track_timer::simulator::scenario_name(context.player.id())
              << " frames=" << options.frames << " resolution=" << kDisplayWidth << 'x'
              << kDisplayHeight << " current=" << model.current_lap.data()
              << " session=" << model.session_remaining.data() << " gnss="
              << model.gnss_status.data() << " logging=" << model.logging_status.data()
              << " fixture=" << gnss.fixture_name()
              << " gnss-rate=" << static_cast<unsigned>(gnss.rate())
              << " gnss-mode=" << track_timer::simulator::gnss_mode_name(gnss.mode())
              << " gnss-dropped=" << diagnostics.gnss.queue.dropped
              << " storage-mode=" << track_timer::simulator::storage_mode_name(storage.mode())
              << " storage-failures=" << storage.status().write_failures
              << " storage-recoveries=" << diagnostics.storage.recoveries
              << " logger-depth=" << logger_metrics.queue.depth
              << " logger-high-water=" << logger_metrics.queue.high_water_mark
              << " logger-dropped=" << logger_metrics.queue.dropped()
              << " logger-storage-unavailable="
              << logger_metrics.storage_unavailable_attempts
              << " logger-write-failures=" << logger_metrics.failed_batch_attempts
              << " logger-p95-us=" << logger_metrics.latency.p95_upper_bound_us
              << " screen=" << track_timer::ui::destination_name(screen.destination())
              << " setup-page=" << track_timer::simulator::setup_page_name(screen.setup_page())
              << " track-catalog-count=" << track_fixture.count
              << " track-state="
              << track_timer::track::track_match_state_name(final_track_match.state)
              << " gate-capture-state="
              << track_timer::ui::gate_capture_status_name(screen.gate_capture().status())
              << " review-state="
              << track_timer::ui::session_review_status_name(
                     screen.session_review().view_model().status)
              << " review-fixture="
              << track_timer::simulator::summary_fixture_name(options.summary_fixture)
              << " diagnostics-state="
              << track_timer::ui::diagnostics_overall_name(diagnostics_snapshot.overall)
              << " diagnostics-fixture="
              << track_timer::simulator::diagnostics_fixture_name(
                     options.diagnostics_fixture)
              << " display-state="
              << track_timer::simulator::display_fixture_name(options.display_fixture)
              << " display-profile="
              << track_timer::ui::brightness_profile_name(
                     screen.display_policy().snapshot().brightness_profile)
              << " display-brightness="
              << static_cast<unsigned>(
                     screen.display_policy().snapshot().command.brightness_percent)
              << " display-orientation="
              << track_timer::ui::display_orientation_name(
                     screen.display_policy().snapshot().command.orientation)
              << " display-dimmed="
              << (screen.display_policy().snapshot().command.dimmed ? "yes" : "no")
              << " display-shift="
              << static_cast<int>(
                     screen.display_policy().snapshot().command.layout_shift_x)
              << ','
              << static_cast<int>(
                     screen.display_policy().snapshot().command.layout_shift_y)
              << " display-preview="
              << (screen.display_policy().snapshot().settings_preview ? "yes" : "no")
              << " imu-fixture="
              << track_timer::simulator::imu_fixture_name(options.imu_fixture)
              << " imu-state="
              << track_timer::ui::imu_meter_state_name(screen.g_meter().snapshot().state)
              << " imu-samples=" << screen.g_meter().snapshot().accepted_samples
              << " imu-trail=" << screen.g_meter().snapshot().trail_count
              << " workflow-fixture=" << workflow_fixture_name(options.workflow_fixture)
              << " workflow-state="
              << track_timer::ui::session_state_name(context.lifecycle.snapshot().state)
              << " trackday-mode="
              << (screen.active_session().view_model().trackday.visible ? "enabled"
                                                                          : "disabled")
              << " trackday-estimate="
              << screen.active_session().view_model().trackday.estimated_laps.data()
              << " lap-feedback="
              << track_timer::ui::lap_feedback_kind_name(
                     screen.active_session().view_model().feedback.kind)
              << " stop-control="
              << track_timer::ui::stop_control_state_name(
                     screen.active_session().view_model().stop.state)
              << " render-frames=" << render_metrics.frame_count
              << " render-average-us=" << render_metrics.average_render_us()
              << " render-maximum-us=" << render_metrics.maximum_render_us
              << " lvgl-maximum-bytes=" << render_metrics.maximum_lvgl_bytes
              << " display-buffer-bytes="
              << track_timer::ui::kDeviceDisplayBuffers.total_bytes() << '\n';

    if (lv_display_get_next(nullptr) != nullptr) {
        lv_group_delete(input_group);
        lv_display_delete(display);
    }
    lv_sdl_quit();
    lv_deinit();
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try {
        return run(parse_options(argc, argv));
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
