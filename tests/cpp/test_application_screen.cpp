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
    simulator::ApplicationScreen screen{lv_screen_active(), settings_manager, tracks.catalog(),
                                        track_match, &summaries};
    ui::ReadySnapshot snapshot{};
    std::strcpy(snapshot.selected_track.data(), "Synthetic Test Loop");
    snapshot.gnss_health = domain::GnssHealth::searching;
    snapshot.storage = ui::Readiness::ready;
    snapshot.imu = ui::Readiness::ready;
    snapshot.logging_available = true;
    screen.update(ui::present_ready(snapshot), ui::DeviceViewModel{});
    lv_obj_update_layout(lv_screen_active());

    assert(screen.destination() == ui::Destination::ready);
    assert(std::strcmp(lv_label_get_text(screen.ready_screen().track_label_object()),
                       "Synthetic Test Loop") == 0);

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
    click(screen.back_button_object());

    click(screen.ready_screen().button_for(ui::NavigationAction::start_session));
    assert(screen.destination() == ui::Destination::active);
    assert(screen.consume_start_request());
    assert(!screen.navigate(ui::NavigationAction::open_setup).accepted);
    assert(screen.destination() == ui::Destination::active);
    assert(screen.navigate(ui::NavigationAction::session_ended).current ==
           ui::Destination::ready);

    lv_display_delete(display);
    lv_sdl_quit();
    lv_deinit();
    std::cout << "Ready dashboard controls and application destinations passed\n";
    return 0;
}
