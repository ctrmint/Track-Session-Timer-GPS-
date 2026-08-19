#include "screen_router.hpp"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "track_timer/catalog/track_catalog.hpp"
#include "track_timer/catalog/track_loader.hpp"
#include "track_timer/diagnostics/snapshot.hpp"
#include "track_timer/storage/sd_card.hpp"
#include "track_timer/storage/sd_track_store.hpp"
#include "track_timer/timing/engine.hpp"
#include "track_timer/display/panel.hpp"
#include "track_timer/ui/diagnostics.hpp"
#include "track_timer/ui/carousel_screen.hpp"
#include "track_timer/ui/diagnostics_screen.hpp"
#include "track_timer/ui/g_radar_screen.hpp"
#include "track_timer/ui/gesture_input.hpp"
#include "track_timer/ui/imu_meter.hpp"
#include "track_timer/imu/calibration.hpp"
#include "track_timer/imu/qmi8658.hpp"
#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/presenter.hpp"
#include "track_timer/ui/ready_screen.hpp"
#include "track_timer/ui/session_review.hpp"
#include "track_timer/ui/session_review_screen.hpp"
#include "track_timer/ui/setup_menu_screen.hpp"
#include "track_timer/ui/shell_navigation.hpp"
#include "track_timer/ui/time_roller_screen.hpp"
#include "track_timer/ui/trackday_screen.hpp"
#include "track_timer/session/controller.hpp"
#include "track_timer/settings/nvs_store.hpp"
#include "track_timer/ui/device_mode.hpp"
#include "track_timer/ui/value_picker.hpp"

#include <lvgl.h>

#include <algorithm>
#include <cstring>
#include <new>
#include <string_view>

namespace track_timer::main_app {
namespace {

using namespace track_timer;  // NOLINT(google-build-using-namespace)

diagnostics::ResetReason translate_reset_reason() noexcept
{
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
        return diagnostics::ResetReason::power_on;
    case ESP_RST_SW:
        return diagnostics::ResetReason::software;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return diagnostics::ResetReason::watchdog;
    case ESP_RST_BROWNOUT:
        return diagnostics::ResetReason::brownout;
    case ESP_RST_PANIC:
        return diagnostics::ResetReason::panic;
    default:
        return diagnostics::ResetReason::unknown;
    }
}

// Real device telemetry. GNSS, storage and IMU stay unavailable because no driver
// exists for them yet; memory and uptime are genuine readings.
diagnostics::DiagnosticsSnapshot device_snapshot() noexcept
{
    diagnostics::DiagnosticsSnapshot snapshot{};
    snapshot.overall = diagnostics::OverallState::degraded;
    snapshot.backend = diagnostics::BackendKind::hardware;
    snapshot.reset_reason = translate_reset_reason();
    snapshot.uptime_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);

    if (const auto* description = esp_app_get_description(); description != nullptr) {
        std::strncpy(snapshot.firmware_version.data(), description->version,
                     snapshot.firmware_version.size() - 1);
    }

    snapshot.internal_ram.state = diagnostics::SubsystemState::ready;
    snapshot.internal_ram.free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.internal_ram.total_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);

    const auto psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    snapshot.psram.state = psram_total > 0 ? diagnostics::SubsystemState::ready
                                           : diagnostics::SubsystemState::unavailable;
    snapshot.psram.free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.psram.total_bytes = psram_total;

    snapshot.gnss = diagnostics::SubsystemState::unavailable;
    snapshot.storage = diagnostics::SubsystemState::unavailable;
    return snapshot;
}

ui::ReadySnapshot ready_snapshot() noexcept
{
    ui::ReadySnapshot snapshot{};
    std::strncpy(snapshot.selected_track.data(), "No track selected",
                 snapshot.selected_track.size() - 1);
    snapshot.session_duration_seconds = 20 * 60;
    snapshot.rest_duration_seconds = 20 * 60;
    snapshot.gnss_health = domain::GnssHealth::unavailable;
    snapshot.storage = ui::Readiness::unavailable;
    snapshot.imu = ui::Readiness::unavailable;
    snapshot.track_state = ui::ReadyTrackState::none;
    return snapshot;
}

// LVGL's symbol set covers these top levels. The settings level still needs the custom
// icon font tracked in issue #128.
// Icons are colour-coded by function so the destination reads before the label does:
// amber for adjustment, azure for stored data and position, green for health.
constexpr std::uint32_t kAmber = 0xFFB020;
constexpr std::uint32_t kAzure = 0x35B0FF;
constexpr std::uint32_t kGreen = 0x3FC98A;
constexpr std::uint32_t kViolet = 0xB06CFF;
constexpr std::uint32_t kRuby = 0xFF8FA3;

// The settings level has no dedicated glyphs in LVGL's built-in set, so related fields
// share an icon. The custom icon font remains open in #128.
[[nodiscard]] const char* icon_for(const ui::SettingsField field) noexcept
{
    switch (field) {
    case ui::SettingsField::session_duration:
    case ui::SettingsField::rest_duration:
    case ui::SettingsField::average_lap:
        return LV_SYMBOL_LOOP;
    case ui::SettingsField::launch_sensitivity:
        return LV_SYMBOL_CHARGE;
    case ui::SettingsField::day_brightness:
    case ui::SettingsField::night_brightness:
    case ui::SettingsField::auto_dim:
        return LV_SYMBOL_EYE_OPEN;
    case ui::SettingsField::lower_display:
        return LV_SYMBOL_LIST;
    case ui::SettingsField::lap_boundary:
    case ui::SettingsField::pit_exit_auto_start:
    case ui::SettingsField::pit_entry_auto_stop:
        return LV_SYMBOL_GPS;
    default:
        return LV_SYMBOL_REFRESH;
    }
}

constexpr ui::CarouselEntry kMenuEntries[] = {
    {LV_SYMBOL_POWER, "MODE", kRuby},
    {LV_SYMBOL_GPS, "TRACK", kAzure},
    {LV_SYMBOL_SETTINGS, "SETUP", kAmber},
    {LV_SYMBOL_LIST, "REVIEW", kAzure},
    {LV_SYMBOL_EYE_OPEN, "DIAGNOSTICS", kGreen},
};

// Track Day is first: it is the safe default, since it withholds the live lap times that
// many track-day regulations prohibit.
constexpr ui::CarouselEntry kModeEntries[] = {
    {LV_SYMBOL_LOOP, "TRACK DAY", kGreen},
    {LV_SYMBOL_PLAY, "RACE", kRuby},
    {LV_SYMBOL_CHARGE, "G-ONLY", kViolet},
};

// Track selection has moved to the top level, so Setup keeps only what belongs there.
constexpr ui::CarouselEntry kSetupEntries[] = {
    {LV_SYMBOL_EDIT, "DEVICE SETTINGS", kAmber},
    {LV_SYMBOL_REFRESH, "G-METER", kViolet},
    {LV_SYMBOL_EYE_OPEN, "DISPLAY", kGreen},
};

class ScreenRouter {
  public:
    // Settings are loaded before any screen is built, so the UI is constructed from what
    // was actually stored rather than from defaults that are then corrected.
    void load_settings() noexcept
    {
        if (!settings_store_.begin()) {
            ESP_LOGE("track_timer", "settings: NVS unavailable; using defaults");
            return;
        }
        const auto report = settings_manager_.load();
        settings_ = settings_manager_.current();
        ESP_LOGI("track_timer", "settings: source=%u current-format=%d, mode %s",
                 static_cast<unsigned>(report.source),
                 static_cast<int>(report.current_format_persisted),
                 ui::device_mode_name(ui::mode_from_settings(settings_)));
    }

    // Every change goes through SettingsManager so validation and the transactional path
    // are the same ones the host tests cover.
    void persist_settings() noexcept
    {
        const auto result = settings_manager_.apply(settings_, false);
        if (result != settings::SettingsApplyResult::applied) {
            ESP_LOGW("track_timer", "settings not saved (apply result %u)",
                     static_cast<unsigned>(result));
            return;
        }
        settings_ = settings_manager_.current();
    }

    // Advances the session clock and refreshes whichever running-session screen is up.
    // Driven from the LVGL service tick so the countdown updates without a task of its
    // own, and so the session clock is never blocked by storage or the card.
    void service_session() noexcept
    {
        const auto now_ms = static_cast<std::int64_t>(esp_timer_get_time() / 1000);
        (void)session_.advance(now_ms);
        const auto snapshot = session_.snapshot();
        const auto active = snapshot.state == session::SessionState::running ||
                            snapshot.state == session::SessionState::overtime;

        shell_.synchronize_session(active);
        if (active != session_was_active_) {
            session_was_active_ = active;
            lv_screen_load(active ? trackday_root_ : home_screen());
        }
        if (!active || trackday_ == nullptr) {
            return;
        }

        domain::UiSnapshot ui_snapshot{};
        ui_snapshot.session_active = true;
        ui_snapshot.session_remaining_ms = snapshot.session_remaining_ms;

        ui::ActiveSessionDisplayConfig display{};
        display.average_lap_seconds = settings_.average_lap_seconds;
        display.trackday_mode_enabled = true;
        display.session_duration_seconds = settings_.session_duration_seconds;

        active_session_.update(ui_snapshot, static_cast<std::uint64_t>(now_ms), display);
        trackday_->update(active_session_.view_model().trackday);
    }

    // Pulls a sample and refreshes the radar. Called from the LVGL task so LVGL is only
    // ever touched by its owner.
    // One entry point for everything the LVGL service tick drives.
    void service() noexcept
    {
        service_imu();
        service_roller();
    }

    void service_imu() noexcept
    {
        board::ImuSample sample{};
        ui::ImuMeterInput input{};
        input.sample_available = imu::read(sample);
        input.now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);

        // Gravity dominates a raw accelerometer reading, so feeding raw axes to the meter
        // parks the dot wherever the unit happens to be tilted. The calibration learns
        // gravity while the device is still, then hands back acceleration with gravity
        // removed and resolved into vehicle axes, whatever angle the unit is mounted at.
        if (input.sample_available) {
            (void)calibration_.update(sample);
        }
        const auto resolved = calibration_.resolve(sample);
        input.calibrating = input.sample_available &&
                            calibration_.state() != imu::CalibrationState::ready;

        // The meter divides its input by g and applies a display rotation, so the
        // resolved values are handed back in m/s2 on the axes it expects.
        board::ImuSample resolved_sample{};
        resolved_sample.monotonic_us = sample.monotonic_us;
        resolved_sample.acceleration_x_mps2 =
            resolved.lateral_g * imu::kStandardGravityMps2;
        resolved_sample.acceleration_y_mps2 =
            resolved.longitudinal_g * imu::kStandardGravityMps2;
        resolved_sample.valid = resolved.valid;
        input.sample = resolved_sample;
        input.sample_available = resolved.valid;
        input.x_axis_valid = resolved.valid;
        input.y_axis_valid = resolved.valid;
        // No session is wired on the device yet, so peaks are not session-scoped here.
        // Session-scoped peaks for Review are tracked in #137.
        service_session();
        const auto& snapshot = imu_meter_.update(input, false);
        // Only when the radar is the visible screen. Repositioning 26 objects and
        // reformatting five labels on every LVGL iteration is wasted work off-screen,
        // and it invalidates areas nobody is looking at.
        if (radar_ != nullptr && radar_root_ != nullptr &&
            lv_screen_active() == radar_root_) {
            radar_->update(snapshot);
        }
    }

    [[nodiscard]] bool build() noexcept
    {
        load_settings();
        (void)build_catalog();
        for (auto*& screen : screens_) {
            screen = lv_obj_create(nullptr);
            if (screen == nullptr) {
                return false;
            }
        }

        // Setup, Review and Diagnostics are reached by holding the dashboard, so their
        // buttons are gone and Start has the space to itself.
        ready_ = new (ready_storage_)
            ui::ReadyScreen(screen_for(ui::Destination::ready), on_ready, this,
                            ui::ReadyControls::start_only);
        setup_ = new (setup_storage_)
            ui::SetupMenuScreen(screen_for(ui::Destination::setup), on_setup, this);
        review_ = new (review_storage_) ui::SessionReviewScreen(
            screen_for(ui::Destination::review), on_review, this);
        diagnostics_ = new (diagnostics_storage_) ui::DiagnosticsScreen(
            screen_for(ui::Destination::diagnostics), on_diagnostics, this);

        // The gated menu lives on its own screen so the Ready dashboard keeps the whole
        // panel for the timer.
        carousel_root_ = lv_obj_create(nullptr);
        if (carousel_root_ == nullptr) {
            return false;
        }
        // G-Only owns the whole panel, so the radar gets its own screen object rather
        // than sharing the dashboard.
        radar_root_ = lv_obj_create(nullptr);
        if (radar_root_ == nullptr) {
            return false;
        }
        trackday_root_ = lv_obj_create(nullptr);
        if (trackday_root_ == nullptr) {
            return false;
        }
        trackday_ = new (trackday_storage_) ui::TrackdayScreen(trackday_root_);
        // A hold still reaches the menu from a running session; the shell refuses to open
        // it while one is live, which is what stops a stray hold ending a session.
        ui::attach_gesture_input(trackday_root_, on_input, this);

        radar_ = new (radar_storage_) ui::GRadarScreen(radar_root_);
        // The hold must still reach the gesture handler with the radar full-panel, which
        // is only true because nothing on it consumes presses.
        ui::attach_gesture_input(radar_root_, on_input, this);

        // The time fields are edited on their own screen: no list of twelve choices can
        // span 0 to 59:59, so they roll rather than pick.
        roller_root_ = lv_obj_create(nullptr);
        if (roller_root_ == nullptr) {
            return false;
        }
        roller_ = new (roller_storage_) ui::TimeRollerScreen(roller_root_);
        ui::attach_gesture_input(roller_root_, on_input, this);
        // Vertical travel drives the columns, so this screen needs the pointer itself
        // rather than the discrete gestures everything else consumes.
        ui::attach_drag_input(roller_root_, on_drag, this);

        carousel_ = new (carousel_storage_)
            ui::CarouselScreen(carousel_root_, on_input, this);
        carousel_->set_entries(kMenuEntries, 4);

        // A hold anywhere on the Ready dashboard opens the menu; the carousel screen
        // takes swipes, presses and the back gesture.
        ui::attach_gesture_input(screen_for(ui::Destination::ready), on_input, this);
        // Start is the largest object on the dashboard and consumes its own presses, so
        // without this a hold on it reaches nothing and the "hold anywhere" hint lies.
        // Its own click handler still starts the session; at Ready level a press is
        // ignored by the shell, so the two cannot conflict.
        if (auto* start = ready_->button_for(ui::NavigationAction::start_session);
            start != nullptr) {
            ui::attach_gesture_input(start, on_input, this);
            ui::bubble_gestures_to_parent(start);
        }
        ui::attach_gesture_input(carousel_root_, on_input, this);

        // Restore the track that was selected before the last reboot.
        restore_selected_track();
        refresh_ready();
        review_controller_.begin(nullptr);  // no storage backend yet
        review_->update(review_controller_.view_model());
        diagnostics_controller_.begin(device_snapshot());
        diagnostics_->update(diagnostics_controller_.view_model());

        lv_screen_load(home_screen());
        return true;
    }

  private:
    [[nodiscard]] lv_obj_t* screen_for(const ui::Destination destination) const noexcept
    {
        const auto index = static_cast<std::size_t>(destination);
        return index < screens_.size() ? screens_[index] : nullptr;
    }

    void go(const ui::NavigationAction action) noexcept
    {
        const auto result = navigation_.dispatch(action);
        if (!result.accepted || result.current == result.previous) {
            return;
        }
        if (result.current == ui::Destination::diagnostics) {
            diagnostics_controller_.update(device_snapshot());
            diagnostics_->update(diagnostics_controller_.view_model());
        }
        if (result.current == ui::Destination::ready) {
            shell_.close();
            lv_screen_load(home_screen());
            return;
        }
        if (auto* target = screen_for(result.current); target != nullptr) {
            lv_screen_load(target);
        }
    }

    static void on_ready(const ui::NavigationAction action, void* context) noexcept
    {
        auto* self = static_cast<ScreenRouter*>(context);
        if (action == ui::NavigationAction::start_session) {
            self->start_session();
            return;
        }
        self->go(action);
    }

    // Only Track Day has a running-session screen so far. Race and G-Only still need
    // their own layouts, tracked on #132.
    void start_session() noexcept
    {
        const auto now_ms = static_cast<std::int64_t>(esp_timer_get_time() / 1000);
        session::SessionConfiguration configuration{};
        configuration.session_duration_ms =
            static_cast<std::int64_t>(settings_.session_duration_seconds) * 1'000;
        configuration.rest_duration_ms =
            static_cast<std::int64_t>(settings_.rest_duration_seconds) * 1'000;
        if (session_.save_configuration(configuration, now_ms) !=
            session::TransitionResult::accepted) {
            (void)session_.enter_configuration(now_ms);
            (void)session_.save_configuration(configuration, now_ms);
        }
        const auto started = session_.start(now_ms);
        ESP_LOGI("track_timer", "session start: %d over %u s", static_cast<int>(started),
                 static_cast<unsigned>(settings_.session_duration_seconds));
        if (trackday_ != nullptr) {
            trackday_->set_track_name(active_track_.name[0] != '\0'
                                          ? active_track_.name.data()
                                          : "TIMER ONLY");
            switch (ui::mode_from_settings(settings_)) {
            case ui::DeviceMode::track_day:
                trackday_->set_mode_note("TRACK DAY  -  lap times available in Review");
                break;
            case ui::DeviceMode::race:
                // Race wants lap times and a delta alongside the countdown. Neither
                // exists without GNSS, so it borrows the countdown and says so.
                trackday_->set_mode_note("RACE  -  lap times pending GNSS");
                break;
            case ui::DeviceMode::g_only:
                trackday_->set_mode_note("G-ONLY  -  session timer");
                break;
            }
        }
    }

    static void on_input(const ui::InputAction action, void* context) noexcept
    {
        static_cast<ScreenRouter*>(context)->handle_input(action);
    }

    // In G-Only the radar is home; otherwise the ready dashboard is.
    [[nodiscard]] lv_obj_t* home_screen() noexcept
    {
        return ui::mode_from_settings(settings_) == ui::DeviceMode::g_only
                   ? radar_root_
                   : screen_for(ui::Destination::ready);
    }

    // The dashboard reflects what is actually loaded, so the driver can confirm the
    // circuit before going out rather than trusting that a selection took.
    [[nodiscard]] ui::ReadySnapshot ready_snapshot() const noexcept
    {
        ui::ReadySnapshot snapshot{};
        snapshot.session_duration_seconds = settings_.session_duration_seconds;
        snapshot.rest_duration_seconds = settings_.rest_duration_seconds;
        snapshot.gnss_health = domain::GnssHealth::unavailable;
        snapshot.storage = storage::mounted() ? ui::Readiness::ready
                                              : ui::Readiness::unavailable;
        snapshot.imu = imu::running() ? ui::Readiness::ready : ui::Readiness::unavailable;
        snapshot.logging_available = false;
        snapshot.session_active = false;

        if (track_armed_) {
            std::strncpy(snapshot.selected_track.data(), active_track_.name.data(),
                         snapshot.selected_track.size() - 1);
            snapshot.track_state = ui::ReadyTrackState::selected;
        }
        else if (active_track_.name[0] != '\0') {
            // Selected and persisted, but the geometry is provisional so timing will not
            // arm. Saying "selected" here would imply lap timing that is not running.
            std::strncpy(snapshot.selected_track.data(), active_track_.name.data(),
                         snapshot.selected_track.size() - 1);
            snapshot.track_state = ui::ReadyTrackState::invalid;
        }
        else {
            std::strncpy(snapshot.selected_track.data(), "No track selected",
                         snapshot.selected_track.size() - 1);
            snapshot.track_state = ui::ReadyTrackState::none;
        }
        return snapshot;
    }

    void refresh_ready() noexcept
    {
        if (ready_ != nullptr) {
            ready_->update(ui::present_ready(ready_snapshot()));
        }
    }

    // The definition array and the parse scratch both live in PSRAM: TrackDefinition is
    // 3.6 KB, so 32 of them is about 115 KB, and a file blob alone is 16 KB. Neither
    // belongs in internal RAM or on a task stack.
    [[nodiscard]] bool build_catalog() noexcept
    {
        auto* storage_array = static_cast<track::TrackDefinition*>(heap_caps_calloc(
            track::kMaximumCatalogTracks, sizeof(track::TrackDefinition),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        load_scratch_ = static_cast<catalog::TrackLoadScratch*>(heap_caps_calloc(
            1, sizeof(catalog::TrackLoadScratch), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (storage_array == nullptr || load_scratch_ == nullptr) {
            ESP_LOGE("track_timer", "no PSRAM for the track catalog; timer-only");
            return false;
        }
        track_catalog_ = catalog::TrackCatalog{storage_array, track::kMaximumCatalogTracks};

        const auto status =
            track_catalog_.rebuild(track_store_, track_store_, load_scratch_->blob);
        ESP_LOGI("track_timer",
                 "track catalog: %s (%u discovered, %u loaded, %u rejected, cap %u)",
                 catalog::catalog_build_result_name(status.result),
                 static_cast<unsigned>(status.discovered),
                 static_cast<unsigned>(status.loaded),
                 static_cast<unsigned>(status.rejected),
                 static_cast<unsigned>(status.capacity));
        if (status.truncated()) {
            ESP_LOGW("track_timer",
                     "track catalog TRUNCATED: the card holds more tracks than the %u-entry "
                     "catalog; some circuits are not selectable",
                     static_cast<unsigned>(status.capacity));
        }
        if (status.loaded == 0) {
            ESP_LOGW("track_timer", "no track geometry under %s; timer-only operation",
                     track_store_.root());
        }
        return true;
    }

    // Reapply whatever was selected before the reboot, so a driver does not have to
    // reselect their circuit every time the device powers up.
    void restore_selected_track() noexcept
    {
        const auto& stored = settings_.selected_track_id;
        const auto length = ::strnlen(stored.data(), stored.size());
        if (length == 0) {
            return;
        }
        const auto index = track_catalog_.find({stored.data(), length});
        if (index == track::kNoTrackIndex) {
            ESP_LOGW("track_timer", "stored track '%s' is not on the card", stored.data());
            return;
        }
        apply_selected_track(index);
    }

    void show_tracks(const ui::ShellState& state) noexcept
    {
        const auto view = track_catalog_.view();
        const auto count = std::min(view.count, track_entries_.size());
        for (std::size_t index = 0; index < count; ++index) {
            const auto& definition = view.definitions[index];
            track_names_[index] = definition.name;
            // Colour carries readiness: a provisional circuit cannot arm timing, and the
            // driver should see that before selecting rather than after.
            track_entries_[index] = {
                LV_SYMBOL_GPS, track_names_[index].data(),
                track::track_timing_ready(definition) ? kGreen : kAmber};
        }
        shell_.set_section_count(count == 0 ? 1 : count);
        carousel_->set_entries(track_entries_.data(), count);
        carousel_->set_title(count == 0 ? "NO TRACKS ON CARD" : "TRACK");
        carousel_->set_position(state.section_index);
    }

    // Reads the chosen definition from the card, parses it and applies it to the timing
    // engine, then persists the choice. Selection is recorded even when the geometry is
    // provisional, so the driver keeps their circuit and simply runs timer-only.
    void apply_selected_track(const std::size_t index) noexcept
    {
        const auto view = track_catalog_.view();
        if (index >= view.count || load_scratch_ == nullptr) {
            return;
        }
        const auto& chosen = view.definitions[index];
        const std::string_view identifier{
            chosen.track_id.data(), ::strnlen(chosen.track_id.data(), chosen.track_id.size())};

        const auto report =
            catalog::apply_track(identifier, track_store_, settings_, false,
                                 timing_engine_, *load_scratch_, active_track_);
        track_armed_ = report.ok();
        if (!track_armed_) {
            // Keep the name so the dashboard can report the circuit and its state.
            active_track_ = chosen;
        }

        std::memset(settings_.selected_track_id.data(), 0,
                    settings_.selected_track_id.size());
        std::memcpy(settings_.selected_track_id.data(), identifier.data(),
                    std::min(identifier.size(), settings_.selected_track_id.size() - 1));
        persist_settings();

        ESP_LOGI("track_timer", "track %s: %s (rev %u, hash %llx)%s", identifier.data(),
                 catalog::track_apply_result_name(report.result),
                 static_cast<unsigned>(report.revision),
                 static_cast<unsigned long long>(report.definition_hash),
                 track_armed_ ? " - timing armed" : " - timer only");
        refresh_ready();
    }

    void show_menu(const ui::ShellState& state) noexcept
    {
        carousel_->set_entries(kMenuEntries, 4);
        carousel_->set_title("MENU");
        carousel_->set_position(state.menu_index);
    }

    void show_section(const ui::ShellState& state) noexcept
    {
        if (shell_.menu_item() == ui::MenuItem::track) {
            show_tracks(state);
            return;
        }
        shell_.set_section_count(ui::kSectionItemCount);
        const auto mode = shell_.menu_item() == ui::MenuItem::mode;
        carousel_->set_entries(mode ? kModeEntries : kSetupEntries, 3);
        // Showing the live mode in the title means the driver can see what is selected
        // before changing it, rather than having to remember.
        carousel_->set_title(mode ? ui::device_mode_label(
                                        ui::mode_from_settings(settings_))
                                  : "SETUP");
        carousel_->set_position(state.section_index);
    }

    void show_fields(const ui::ShellState& state) noexcept
    {
        for (std::size_t index = 0; index < ui::kPickerFields.size(); ++index) {
            const auto field = ui::kPickerFields[index];
            field_entries_[index] = {icon_for(field), ui::picker_field_label(field),
                                     kAmber};
        }
        carousel_->set_entries(field_entries_.data(), field_entries_.size());
        carousel_->set_title("DEVICE SETTINGS");
        carousel_->set_position(state.field_index);
    }

    // Every value the field accepts, each one press away. This is what replaces the
    // increment/decrement stepping that needed up to 150 presses for an average lap.
    void show_values(const ui::ShellState& state, const bool reset_to_current) noexcept
    {
        const auto field = ui::kPickerFields[state.field_index];
        if (ui::is_time_field(field)) {
            if (roller_ != nullptr) {
                roller_->configure(ui::picker_field_label(field), field,
                                   ui::time_field_seconds(field, settings_));
                roller_field_ = field;
                roller_active_ = true;
                roller_serviced_ms_ = lv_tick_get();
                lv_screen_load(roller_root_);
            }
            return;
        }
        roller_active_ = false;
        const auto list = ui::choices_for(field, settings_);
        for (std::size_t index = 0; index < list.count; ++index) {
            value_text_[index] = list.choices[index].text;
            value_entries_[index] = {icon_for(field), value_text_[index].data(), kAzure};
        }
        shell_.set_value_count(list.count);
        carousel_->set_entries(value_entries_.data(), list.count);
        carousel_->set_title(ui::picker_field_label(field));
        carousel_->set_position(reset_to_current ? list.selected : state.value_index);
        if (reset_to_current) {
            shell_.select_value(list.selected);
        }
    }

    static void on_drag(const ui::DragSample& sample, void* const context) noexcept
    {
        static_cast<ScreenRouter*>(context)->handle_drag(sample);
    }

    void handle_drag(const ui::DragSample& sample) noexcept
    {
        if (!roller_active_ || roller_ == nullptr) {
            return;
        }
        switch (sample.phase) {
        case ui::DragPhase::began:
            // The column is chosen by where the touch lands, so minutes and seconds are
            // both adjustable without switching focus between them.
            roller_->roller().begin(roller_->column_at(sample.x));
            hold_.begin(lv_tick_get());
            roller_->set_hold_progress(0.0F);
            break;
        case ui::DragPhase::moved:
            roller_->roller().drag(static_cast<float>(sample.dy), sample.elapsed_ms);
            hold_.travel(sample.dy);
            roller_->refresh();
            break;
        case ui::DragPhase::ended:
            roller_->roller().release();
            hold_.end();
            roller_->set_hold_progress(0.0F);
            roller_serviced_ms_ = lv_tick_get();
            break;
        }
    }

    // Carries a flick after the finger has gone. Driven from the service tick rather than
    // a timer of its own, so it cannot outlive the screen that owns it.
    void service_roller() noexcept
    {
        if (!roller_active_ || roller_ == nullptr) {
            return;
        }
        // A save is a hold measured here rather than LVGL's 400 ms long press, which was
        // eager enough to commit a value the driver was still rolling.
        if (hold_.active()) {
            const auto now = lv_tick_get();
            roller_->set_hold_progress(hold_.progress(now));
            if (hold_.complete(now)) {
                hold_.end();
                roller_->set_hold_progress(0.0F);
                leave_roller(true);
                return;
            }
        }
        if (!roller_->roller().coasting()) {
            return;
        }
        const auto now_ms = lv_tick_get();
        const auto elapsed = now_ms - roller_serviced_ms_;
        roller_serviced_ms_ = now_ms;
        (void)roller_->roller().advance(elapsed);
        roller_->refresh();
    }

    // Leaves the roller for the field list, saving first when asked. Cancelling is a
    // horizontal swipe because down now drives the digits and can no longer mean back.
    void leave_roller(const bool commit) noexcept
    {
        if (roller_ != nullptr && commit) {
            if (ui::apply_time_field(roller_field_, roller_->roller().committed_seconds(),
                                     settings_)) {
                persist_settings();
                ESP_LOGI("track_timer", "set %s to %u s",
                         ui::picker_field_label(roller_field_),
                         static_cast<unsigned>(
                             ui::time_field_seconds(roller_field_, settings_)));
            }
        }
        roller_active_ = false;
        hold_.end();
        const auto result = shell_.dispatch(ui::InputAction::swipe_down);
        if (result.state.level == ui::ShellLevel::field) {
            show_fields(result.state);
            lv_screen_load(carousel_root_);
            return;
        }
        lv_screen_load(home_screen());
    }

    void handle_input(const ui::InputAction action) noexcept
    {
        if (roller_active_ && roller_ != nullptr) {
            switch (action) {
            case ui::InputAction::long_press:
                // Saving is driven by the measured hold in service_roller, not by LVGL's
                // 400 ms threshold.
                return;
            case ui::InputAction::swipe_left:
            case ui::InputAction::swipe_right:
                leave_roller(false);
                return;
            case ui::InputAction::swipe_up:
                roller_->roller().step(roller_->roller().active(), 1);
                roller_->refresh();
                return;
            case ui::InputAction::swipe_down:
                roller_->roller().step(roller_->roller().active(), -1);
                roller_->refresh();
                return;
            default:
                // A tap changes nothing here: saving is deliberate, and a stray press
                // must not commit a half-rolled value.
                return;
            }
        }
        const auto result = shell_.dispatch(action);
        switch (result.outcome) {
        case ui::ShellOutcome::menu_opened:
            show_menu(result.state);
            lv_screen_load(carousel_root_);
            break;
        case ui::ShellOutcome::moved:
            carousel_->set_position(position_for(result.state));
            break;
        case ui::ShellOutcome::entered:
            switch (result.state.level) {
            case ui::ShellLevel::section:
                show_section(result.state);
                break;
            case ui::ShellLevel::field:
                show_fields(result.state);
                break;
            case ui::ShellLevel::value:
                show_values(result.state, true);
                break;
            default:
                if (result.emits_action) {
                    go(result.action);
                }
                break;
            }
            break;
        case ui::ShellOutcome::mode_selected:
            ui::apply_mode(static_cast<ui::DeviceMode>(result.state.section_index),
                           settings_);
            persist_settings();
            ESP_LOGI("track_timer", "mode: %s",
                     ui::device_mode_name(ui::mode_from_settings(settings_)));
            shell_.close();
            lv_screen_load(home_screen());
            break;
        case ui::ShellOutcome::track_selected:
            apply_selected_track(result.state.section_index);
            shell_.close();
            lv_screen_load(home_screen());
            break;
        case ui::ShellOutcome::value_selected:
            if (ui::apply_choice(ui::kPickerFields[result.state.field_index],
                                 result.state.value_index, settings_)) {
                persist_settings();
                ESP_LOGI("track_timer", "set %s",
                         ui::picker_field_label(
                             ui::kPickerFields[result.state.field_index]));
            }
            show_values(result.state, true);
            break;
        case ui::ShellOutcome::exited:
            switch (result.state.level) {
            case ui::ShellLevel::menu:
                show_menu(result.state);
                break;
            case ui::ShellLevel::section:
                show_section(result.state);
                break;
            case ui::ShellLevel::field:
                show_fields(result.state);
                break;
            default:
                lv_screen_load(home_screen());
                break;
            }
            break;
        case ui::ShellOutcome::refused_session_active:
        case ui::ShellOutcome::ignored:
            break;
        }
    }

    [[nodiscard]] static std::size_t position_for(const ui::ShellState& state) noexcept
    {
        switch (state.level) {
        case ui::ShellLevel::menu:
            return state.menu_index;
        case ui::ShellLevel::section:
            return state.section_index;
        case ui::ShellLevel::field:
            return state.field_index;
        case ui::ShellLevel::value:
            return state.value_index;
        default:
            return 0;
        }
    }

    static void on_setup(const ui::SetupMenuAction action, void* context) noexcept
    {
        // Device settings, track selection and the G-meter are separate screens that
        // are not routed yet; only Back leaves this screen.
        if (action == ui::SetupMenuAction::back) {
            static_cast<ScreenRouter*>(context)->go(ui::NavigationAction::back);
        }
    }

    static void on_review(const ui::SessionReviewAction action, void* context) noexcept
    {
        auto* self = static_cast<ScreenRouter*>(context);
        switch (action) {
        case ui::SessionReviewAction::return_to_ready:
        case ui::SessionReviewAction::return_to_rest:
            self->go(ui::NavigationAction::back);
            break;
        case ui::SessionReviewAction::newer_session:
            self->review_controller_.newer_session();
            self->review_->update(self->review_controller_.view_model());
            break;
        case ui::SessionReviewAction::older_session:
            self->review_controller_.older_session();
            self->review_->update(self->review_controller_.view_model());
            break;
        case ui::SessionReviewAction::previous_page:
            self->review_controller_.previous_lap_page();
            self->review_->update(self->review_controller_.view_model());
            break;
        case ui::SessionReviewAction::next_page:
            self->review_controller_.next_lap_page();
            self->review_->update(self->review_controller_.view_model());
            break;
        }
    }

    static void on_diagnostics(const ui::DiagnosticsAction action, void* context) noexcept
    {
        auto* self = static_cast<ScreenRouter*>(context);
        switch (action) {
        case ui::DiagnosticsAction::back:
            self->go(ui::NavigationAction::back);
            break;
        case ui::DiagnosticsAction::previous_page:
            self->diagnostics_controller_.previous_page();
            self->diagnostics_->update(self->diagnostics_controller_.view_model());
            break;
        case ui::DiagnosticsAction::next_page:
            self->diagnostics_controller_.next_page();
            self->diagnostics_->update(self->diagnostics_controller_.view_model());
            break;
        }
    }

    ui::NavigationController navigation_{};
    ui::ShellNavigation shell_{ui::kPickerFields.size()};
    settings::NvsSettingsStore settings_store_{};
    settings::SettingsManager settings_manager_{settings_store_};
    settings::DeviceSettings settings_{};
    std::array<ui::CarouselEntry, ui::kPickerFields.size()> field_entries_{};
    std::array<ui::CarouselEntry, ui::kValueChoiceCapacity> value_entries_{};
    std::array<std::array<char, ui::kValueTextCapacity>, ui::kValueChoiceCapacity>
        value_text_{};
    ui::SessionReviewController review_controller_{};
    ui::DiagnosticsController diagnostics_controller_{};
    std::array<lv_obj_t*, 6> screens_{};

    ui::ReadyScreen* ready_{nullptr};
    ui::SetupMenuScreen* setup_{nullptr};
    ui::SessionReviewScreen* review_{nullptr};
    ui::DiagnosticsScreen* diagnostics_{nullptr};
    ui::CarouselScreen* carousel_{nullptr};
    lv_obj_t* carousel_root_{nullptr};
    ui::GRadarScreen* radar_{nullptr};
    lv_obj_t* radar_root_{nullptr};
    ui::TrackdayScreen* trackday_{nullptr};
    lv_obj_t* trackday_root_{nullptr};
    ui::TimeRollerScreen* roller_{nullptr};
    ui::SettingsField roller_field_{ui::SettingsField::average_lap};
    lv_obj_t* roller_root_{nullptr};
    bool roller_active_{false};
    ui::HoldToSave hold_{};
    std::uint32_t roller_serviced_ms_{0};
    session::SessionController session_{};
    ui::ActiveSessionController active_session_{};
    bool session_was_active_{false};
    ui::ImuMeterController imu_meter_{};
    track::TrackDefinition active_track_{};
    bool track_armed_{false};
    storage::SdTrackStore track_store_{};
    catalog::TrackCatalog track_catalog_{nullptr, 0};
    catalog::TrackLoadScratch* load_scratch_{nullptr};
    timing::TimingEngine timing_engine_{};
    std::array<ui::CarouselEntry, track::kMaximumCatalogTracks> track_entries_{};
    std::array<std::array<char, track::kTrackNameCapacity>,
               track::kMaximumCatalogTracks> track_names_{};
    imu::GravityCalibration calibration_{};

    alignas(ui::ReadyScreen) std::byte ready_storage_[sizeof(ui::ReadyScreen)]{};
    alignas(ui::SetupMenuScreen) std::byte setup_storage_[sizeof(ui::SetupMenuScreen)]{};
    alignas(ui::SessionReviewScreen) std::byte
        review_storage_[sizeof(ui::SessionReviewScreen)]{};
    alignas(ui::DiagnosticsScreen) std::byte
        diagnostics_storage_[sizeof(ui::DiagnosticsScreen)]{};
    alignas(ui::CarouselScreen) std::byte carousel_storage_[sizeof(ui::CarouselScreen)]{};
    alignas(ui::GRadarScreen) std::byte radar_storage_[sizeof(ui::GRadarScreen)]{};
    alignas(ui::TrackdayScreen) std::byte trackday_storage_[sizeof(ui::TrackdayScreen)]{};
    alignas(ui::TimeRollerScreen) std::byte
        roller_storage_[sizeof(ui::TimeRollerScreen)]{};
};

ScreenRouter router{};

}  // namespace

// Exposed so the LVGL task can drive the radar without the router owning a task.
void service_screen_router() noexcept { router.service(); }

bool start_screen_router() noexcept
{
    if (!display::lock(1000)) {
        return false;
    }
    const auto built = router.build();
    display::unlock();
    return built;
}

}  // namespace track_timer::main_app
