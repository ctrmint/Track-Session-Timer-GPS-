#pragma once

#include "device_screen.hpp"

#include "track_timer/settings/settings.hpp"
#include "track_timer/logger/summary_provider.hpp"
#include "track_timer/track/matching.hpp"
#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/diagnostics.hpp"
#include "track_timer/ui/diagnostics_screen.hpp"
#include "track_timer/ui/presenter.hpp"
#include "track_timer/ui/ready_screen.hpp"
#include "track_timer/ui/setup_menu_screen.hpp"
#include "track_timer/ui/settings_editor.hpp"
#include "track_timer/ui/settings_screen.hpp"
#include "track_timer/ui/session_review.hpp"
#include "track_timer/ui/session_review_screen.hpp"
#include "track_timer/ui/track_selection.hpp"
#include "track_timer/ui/track_selection_screen.hpp"

#include <lvgl.h>

namespace track_timer::simulator {

enum class SetupPage : std::uint8_t {
    menu,
    device_settings,
    track_selection,
};

class ApplicationScreen {
  public:
    ApplicationScreen(lv_obj_t* root, settings::SettingsManager& settings_manager,
                      track::TrackCatalogView track_catalog,
                      const track::TrackMatchResult& track_match,
                      logger::SessionSummaryProvider* summary_provider) noexcept;

    void update(const ui::ReadyViewModel& ready, const ui::DeviceViewModel& active,
                const diagnostics::DiagnosticsSnapshot& diagnostics) noexcept;
    [[nodiscard]] ui::NavigationResult navigate(ui::NavigationAction action) noexcept;
    void synchronize_session(bool active) noexcept;
    void update_track_match(const track::TrackMatchResult& match) noexcept;
    void open_setup_page(SetupPage page) noexcept;
    [[nodiscard]] bool consume_start_request() noexcept;
    [[nodiscard]] bool consume_rest_request() noexcept;
    [[nodiscard]] bool consume_ready_request() noexcept;
    void add_controls_to_group(lv_group_t* group) noexcept;

    [[nodiscard]] ui::Destination destination() const noexcept;
    [[nodiscard]] ui::ReadyScreen& ready_screen() noexcept;
    [[nodiscard]] ui::SetupMenuScreen& setup_menu_screen() noexcept;
    [[nodiscard]] ui::SettingsScreen& settings_screen() noexcept;
    [[nodiscard]] const ui::SettingsEditor& settings_editor() const noexcept;
    [[nodiscard]] ui::TrackSelectionScreen& track_selection_screen() noexcept;
    [[nodiscard]] const ui::TrackSelectionController& track_selection() const noexcept;
    [[nodiscard]] ui::SessionReviewScreen& session_review_screen() noexcept;
    [[nodiscard]] const ui::SessionReviewController& session_review() const noexcept;
    [[nodiscard]] ui::DiagnosticsScreen& diagnostics_screen() noexcept;
    [[nodiscard]] const ui::DiagnosticsController& diagnostics() const noexcept;
    [[nodiscard]] SetupPage setup_page() const noexcept;

    ApplicationScreen(const ApplicationScreen&) = delete;
    ApplicationScreen& operator=(const ApplicationScreen&) = delete;

  private:
    static void ready_navigation(ui::NavigationAction action, void* context) noexcept;
    static void setup_action(ui::SetupMenuAction action, void* context) noexcept;
    static void settings_action(ui::SettingsScreenAction action, void* context) noexcept;
    static void track_action(ui::TrackSelectionAction action, void* context) noexcept;
    static void review_action(ui::SessionReviewAction action, void* context) noexcept;
    static void diagnostics_action(ui::DiagnosticsAction action, void* context) noexcept;
    void show_destination() noexcept;
    void refresh_settings() noexcept;
    void refresh_track_selection() noexcept;
    void refresh_session_review() noexcept;
    void refresh_diagnostics() noexcept;

    settings::SettingsManager& settings_manager_;
    track::TrackCatalogView track_catalog_{};
    track::TrackMatchResult track_match_{};
    diagnostics::DiagnosticsSnapshot diagnostics_snapshot_{};
    logger::SessionSummaryProvider* summary_provider_{nullptr};
    ui::NavigationController navigation_{};
    SetupPage setup_page_{SetupPage::menu};
    bool start_requested_{false};
    bool rest_requested_{false};
    bool ready_requested_{false};
    bool session_active_{false};
    lv_obj_t* ready_root_{nullptr};
    lv_obj_t* active_root_{nullptr};
    lv_obj_t* setup_menu_root_{nullptr};
    lv_obj_t* settings_root_{nullptr};
    lv_obj_t* track_selection_root_{nullptr};
    lv_obj_t* session_review_root_{nullptr};
    lv_obj_t* diagnostics_root_{nullptr};
    ui::ReadyScreen ready_screen_;
    DeviceScreen active_screen_;
    ui::SetupMenuScreen setup_menu_screen_;
    ui::SettingsEditor settings_editor_{};
    ui::SettingsScreen settings_screen_;
    ui::TrackSelectionController track_selection_{};
    ui::TrackSelectionScreen track_selection_screen_;
    ui::SessionReviewController session_review_{};
    ui::SessionReviewScreen session_review_screen_;
    ui::DiagnosticsController diagnostics_{};
    ui::DiagnosticsScreen diagnostics_screen_;
};

[[nodiscard]] const char* setup_page_name(SetupPage page) noexcept;

}  // namespace track_timer::simulator
