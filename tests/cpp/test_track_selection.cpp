#include "track_timer/settings/settings.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/track/matching.hpp"
#include "track_timer/ui/track_selection.hpp"

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
        return !fail_writes;
    }

    bool fail_writes{false};
};

track_timer::track::TrackMatchResult match_fixture(
    const track_timer::simulator::TrackFixture& fixture,
    const track_timer::settings::DeviceSettings& settings)
{
    auto request = fixture.request;
    request.selected_track_id = settings.selected_track_id.data();
    return track_timer::track::match_track_geofences(fixture.catalog(), request);
}

}  // namespace

int main()
{
    using namespace track_timer;

    MemoryStore store{};
    settings::SettingsManager manager{store};
    (void)manager.load();
    ui::TrackSelectionController controller{};

    const auto suggested = simulator::make_track_fixture(
        simulator::TrackFixtureId::suggested);
    controller.begin(suggested.catalog(), match_fixture(suggested, manager.current()),
                     manager.current(), false);
    assert(controller.status() == ui::TrackSelectionStatus::suggested);
    assert(controller.browse_index() == 0);
    auto view = controller.view_model();
    assert(view.can_browse && view.can_select && view.can_use_timer_only);
    assert(std::strcmp(view.track_name.data(), "Synthetic Test Loop") == 0);
    assert(std::strstr(view.definition.data(), "START/FINISH READY") != nullptr);

    controller.next();
    assert(controller.browse_index() == 1);
    assert(controller.status() == ui::TrackSelectionStatus::browsing);
    controller.previous();
    assert(controller.browse_index() == 0);
    assert(controller.select(manager, false) == settings::SettingsApplyResult::applied);
    assert(controller.status() == ui::TrackSelectionStatus::saved);
    assert(std::strcmp(manager.current().selected_track_id.data(),
                       "synthetic_test_loop") == 0);

    controller.show_capture_information();
    assert(controller.status() == ui::TrackSelectionStatus::capture_information);
    assert(std::strcmp(manager.current().selected_track_id.data(),
                       "synthetic_test_loop") == 0);
    assert(controller.use_timer_only(manager, false) == settings::SettingsApplyResult::applied);
    assert(controller.status() == ui::TrackSelectionStatus::timer_only);
    assert(manager.current().selected_track_id[0] == '\0');

    struct FixtureExpectation {
        simulator::TrackFixtureId fixture;
        ui::TrackSelectionStatus status;
    };
    for (const auto expected : {
             FixtureExpectation{simulator::TrackFixtureId::missing,
                                ui::TrackSelectionStatus::selected_track_missing},
             FixtureExpectation{simulator::TrackFixtureId::invalid,
                                ui::TrackSelectionStatus::invalid_catalog},
             FixtureExpectation{simulator::TrackFixtureId::ambiguous,
                                ui::TrackSelectionStatus::ambiguous},
             FixtureExpectation{simulator::TrackFixtureId::suggested,
                                ui::TrackSelectionStatus::suggested},
             FixtureExpectation{simulator::TrackFixtureId::none,
                                ui::TrackSelectionStatus::no_nearby_track},
             FixtureExpectation{simulator::TrackFixtureId::unavailable,
                                ui::TrackSelectionStatus::location_unavailable},
         }) {
        const auto fixture = simulator::make_track_fixture(expected.fixture);
        auto current = manager.current();
        current.selected_track_id.fill('\0');
        if (!fixture.request.selected_track_id.empty()) {
            std::snprintf(current.selected_track_id.data(), current.selected_track_id.size(),
                          "%.*s", static_cast<int>(fixture.request.selected_track_id.size()),
                          fixture.request.selected_track_id.data());
        }
        controller.begin(fixture.catalog(), match_fixture(fixture, current), current, false);
        assert(controller.status() == expected.status);
    }

    auto provisional = suggested;
    provisional.definitions[0].provenance.geometry_status =
        track::TrackGeometryStatus::provisional;
    auto timer_only_settings = manager.current();
    timer_only_settings.selected_track_id.fill('\0');
    controller.begin(provisional.catalog(),
                     match_fixture(provisional, timer_only_settings),
                     timer_only_settings, false);
    view = controller.view_model();
    assert(view.can_browse && !view.can_select);
    assert(std::strstr(view.definition.data(), "PROVISIONAL - TIMER ONLY") != nullptr);
    assert(controller.select(manager, false) ==
           settings::SettingsApplyResult::invalid_settings);
    assert(controller.status() == ui::TrackSelectionStatus::provisional);

    auto selected_settings = manager.current();
    std::strcpy(selected_settings.selected_track_id.data(), "synthetic_test_loop");
    assert(manager.apply(selected_settings, false) == settings::SettingsApplyResult::applied);
    const auto selected = simulator::make_track_fixture(simulator::TrackFixtureId::selected);
    controller.begin(selected.catalog(), match_fixture(selected, manager.current()),
                     manager.current(), false);
    assert(controller.status() == ui::TrackSelectionStatus::saved);

    controller.begin(selected.catalog(), match_fixture(selected, manager.current()),
                     manager.current(), true);
    assert(controller.status() == ui::TrackSelectionStatus::locked_active);
    assert(controller.select(manager, true) == settings::SettingsApplyResult::invalid_settings);
    assert(!manager.has_pending_change());

    controller.begin(suggested.catalog(), match_fixture(suggested, manager.current()),
                     manager.current(), false);
    controller.next();
    assert(controller.select(manager, true) == settings::SettingsApplyResult::invalid_settings);
    assert(std::strcmp(manager.current().selected_track_id.data(),
                       "synthetic_test_loop") == 0);
    assert(!manager.has_pending_change());

    controller.begin(suggested.catalog(), match_fixture(suggested, manager.current()),
                     manager.current(), false);
    store.fail_writes = true;
    assert(controller.use_timer_only(manager, false) ==
           settings::SettingsApplyResult::storage_error);
    assert(controller.status() == ui::TrackSelectionStatus::storage_error);
    assert(std::strcmp(manager.current().selected_track_id.data(),
                       "synthetic_test_loop") == 0);

    for (const auto name : {"selected", "missing", "invalid", "ambiguous", "suggested",
                            "none", "unavailable"}) {
        simulator::TrackFixtureId id{};
        assert(simulator::parse_track_fixture(name, id));
        assert(std::strcmp(simulator::track_fixture_name(id), name) == 0);
    }

    std::cout << "Track selection states, persistence, and active-session lock passed\n";
    return 0;
}
