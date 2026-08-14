#include "track_timer/simulator/track_fixtures.hpp"

#include <cstdio>

namespace track_timer::simulator {
namespace {

track::DirectedGateDefinition make_gate(const double latitude, const double longitude,
                                        const double latitude_offset) noexcept
{
    track::DirectedGateDefinition gate{};
    gate.left = {latitude + latitude_offset, longitude - 0.00002};
    gate.right = {latitude - latitude_offset, longitude + 0.00002};
    gate.direction_heading_deg = 90.0;
    gate.heading_tolerance_deg = 60.0;
    gate.minimum_crossing_speed_mps = 2.0;
    gate.rearm_corridor_m = 15.0;
    return gate;
}

track::TrackDefinition make_definition(const char* id, const char* name,
                                       const double latitude, const double longitude,
                                       const double radius_m, const std::uint64_t hash) noexcept
{
    track::TrackDefinition definition{};
    definition.schema_version = track::kCurrentTrackSchemaVersion;
    std::snprintf(definition.track_id.data(), definition.track_id.size(), "%s", id);
    std::snprintf(definition.name.data(), definition.name.size(), "%s", name);
    std::snprintf(definition.country.data(), definition.country.size(), "XX");
    definition.revision = 1;
    std::snprintf(definition.provenance.source.data(),
                  definition.provenance.source.size(), "Simulator fixture");
    std::snprintf(definition.provenance.license.data(),
                  definition.provenance.license.size(), "CC0-1.0");
    std::snprintf(definition.provenance.verified_utc.data(),
                  definition.provenance.verified_utc.size(),
                  "2026-08-14T00:00:00Z");
    definition.provenance.geometry_status =
        track::TrackGeometryStatus::physically_validated;
    definition.reference = {latitude, longitude};
    definition.geofence = {{latitude, longitude}, radius_m};
    definition.gates.start = make_gate(latitude, longitude, 0.00005);
    definition.gates.finish = definition.gates.start;
    definition.gates.pit_entry = make_gate(latitude + 0.0002, longitude, 0.00005);
    definition.gates.pit_exit = make_gate(latitude - 0.0002, longitude, 0.00005);
    definition.minimum_lap_time_s = 20.0;
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
