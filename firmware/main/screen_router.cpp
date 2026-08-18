#include "screen_router.hpp"

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "track_timer/diagnostics/snapshot.hpp"
#include "track_timer/display/panel.hpp"
#include "track_timer/ui/diagnostics.hpp"
#include "track_timer/ui/diagnostics_screen.hpp"
#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/presenter.hpp"
#include "track_timer/ui/ready_screen.hpp"
#include "track_timer/ui/session_review.hpp"
#include "track_timer/ui/session_review_screen.hpp"
#include "track_timer/ui/setup_menu_screen.hpp"

#include <lvgl.h>

#include <cstring>
#include <new>

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
    snapshot.session_duration_minutes = 20;
    snapshot.rest_duration_minutes = 20;
    snapshot.gnss_health = domain::GnssHealth::unavailable;
    snapshot.storage = ui::Readiness::unavailable;
    snapshot.imu = ui::Readiness::unavailable;
    snapshot.track_state = ui::ReadyTrackState::none;
    return snapshot;
}

class ScreenRouter {
  public:
    [[nodiscard]] bool build() noexcept
    {
        for (auto*& screen : screens_) {
            screen = lv_obj_create(nullptr);
            if (screen == nullptr) {
                return false;
            }
        }

        ready_ = new (ready_storage_)
            ui::ReadyScreen(screen_for(ui::Destination::ready), on_ready, this);
        setup_ = new (setup_storage_)
            ui::SetupMenuScreen(screen_for(ui::Destination::setup), on_setup, this);
        review_ = new (review_storage_) ui::SessionReviewScreen(
            screen_for(ui::Destination::review), on_review, this);
        diagnostics_ = new (diagnostics_storage_) ui::DiagnosticsScreen(
            screen_for(ui::Destination::diagnostics), on_diagnostics, this);

        ready_->update(ui::present_ready(ready_snapshot()));
        review_controller_.begin(nullptr);  // no storage backend yet
        review_->update(review_controller_.view_model());
        diagnostics_controller_.begin(device_snapshot());
        diagnostics_->update(diagnostics_controller_.view_model());

        lv_screen_load(screen_for(ui::Destination::ready));
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
        if (auto* target = screen_for(result.current); target != nullptr) {
            lv_screen_load(target);
        }
    }

    static void on_ready(const ui::NavigationAction action, void* context) noexcept
    {
        static_cast<ScreenRouter*>(context)->go(action);
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
    ui::SessionReviewController review_controller_{};
    ui::DiagnosticsController diagnostics_controller_{};
    std::array<lv_obj_t*, 6> screens_{};

    ui::ReadyScreen* ready_{nullptr};
    ui::SetupMenuScreen* setup_{nullptr};
    ui::SessionReviewScreen* review_{nullptr};
    ui::DiagnosticsScreen* diagnostics_{nullptr};

    alignas(ui::ReadyScreen) std::byte ready_storage_[sizeof(ui::ReadyScreen)]{};
    alignas(ui::SetupMenuScreen) std::byte setup_storage_[sizeof(ui::SetupMenuScreen)]{};
    alignas(ui::SessionReviewScreen) std::byte
        review_storage_[sizeof(ui::SessionReviewScreen)]{};
    alignas(ui::DiagnosticsScreen) std::byte
        diagnostics_storage_[sizeof(ui::DiagnosticsScreen)]{};
};

ScreenRouter router{};

}  // namespace

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
