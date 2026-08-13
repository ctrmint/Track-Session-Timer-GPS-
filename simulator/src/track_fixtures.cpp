#include "track_timer/simulator/track_fixtures.hpp"

#include <cstdio>

namespace track_timer::simulator {
namespace {

track::TrackDefinition make_definition(const char* id, const char* name,
                                       const double latitude, const double longitude,
                                       const double radius_m, const std::uint64_t hash) noexcept
{
    track::TrackDefinition definition{};
    definition.schema_version = track::kCurrentTrackSchemaVersion;
    std::snprintf(definition.track_id.data(), definition.track_id.size(), "%s", id);
    std::snprintf(definition.name.data(), definition.name.size(), "%s", name);
    std::snprintf(definition.country.data(), definition.country.size(), "XX");
    definition.reference = {latitude, longitude};
    definition.geofence = {{latitude, longitude}, radius_m};
    definition.start_finish.a = {latitude + 0.00005, longitude - 0.00002};
    definition.start_finish.b = {latitude - 0.00005, longitude + 0.00002};
    definition.start_finish.direction_heading_deg = 90.0;
    definition.start_finish.heading_tolerance_deg = 60.0;
    definition.start_finish.minimum_lap_time_s = 20.0;
    definition.definition_hash = hash;
    return definition;
}

}  // namespace

track::TrackCatalogView TrackFixture::catalog() const noexcept
{
    return {definitions.data(), count};
}

TrackFixture make_track_fixture(const TrackFixtureId id) noexcept
{
    TrackFixture fixture{};
    fixture.definitions[0] = make_definition("synthetic_test_loop", "Synthetic Test Loop",
                                             52.0, -1.0, 2'000.0,
                                             0x53796E7468657469ULL);
    fixture.definitions[1] = make_definition("adjacent_test_loop", "Adjacent Test Loop",
                                             52.03, -1.0, 1'000.0,
                                             0x41646A6163656E74ULL);
    fixture.count = 2;
    fixture.request.position = {52.0, -1.0};
    fixture.request.position_valid = true;

    switch (id) {
    case TrackFixtureId::selected:
        fixture.request.selected_track_id = "synthetic_test_loop";
        break;
    case TrackFixtureId::missing:
        fixture.request.selected_track_id = "removed_test_loop";
        break;
    case TrackFixtureId::invalid:
        fixture.definitions[0].schema_version = 99;
        break;
    case TrackFixtureId::ambiguous:
        fixture.definitions[1].geofence.center = {52.005, -1.0};
        fixture.definitions[1].geofence.radius_m = 2'000.0;
        fixture.request.position = {52.0025, -1.0};
        break;
    case TrackFixtureId::suggested:
        break;
    case TrackFixtureId::none:
        fixture.request.position = {54.0, -1.0};
        break;
    case TrackFixtureId::unavailable:
        fixture.request.position_valid = false;
        break;
    }
    return fixture;
}

bool parse_track_fixture(const std::string_view text, TrackFixtureId& id) noexcept
{
    constexpr std::array<std::string_view, 7> names{
        "selected", "missing", "invalid", "ambiguous", "suggested", "none", "unavailable",
    };
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (text == names[index]) {
            id = static_cast<TrackFixtureId>(index);
            return true;
        }
    }
    return false;
}

const char* track_fixture_name(const TrackFixtureId id) noexcept
{
    constexpr std::array<const char*, 7> names{
        "selected", "missing", "invalid", "ambiguous", "suggested", "none", "unavailable",
    };
    return names[static_cast<std::size_t>(id)];
}

}  // namespace track_timer::simulator
