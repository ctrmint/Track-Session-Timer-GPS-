#include "track_timer/track/matching.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

track_timer::track::TrackDefinition make_track(const char* id, const double latitude,
                                                const double longitude,
                                                const double radius_m)
{
    track_timer::track::TrackDefinition definition{};
    definition.schema_version = track_timer::track::kCurrentTrackSchemaVersion;
    std::strcpy(definition.track_id.data(), id);
    std::strcpy(definition.name.data(), id);
    definition.geofence.center = {latitude, longitude};
    definition.geofence.radius_m = radius_m;
    return definition;
}

}  // namespace

int main()
{
    using namespace track_timer::track;

    const auto one_degree = geographic_distance_m({0.0, 0.0}, {1.0, 0.0});
    assert(std::abs(one_degree - 111'194.9) < 1.0);
    assert(std::abs(geographic_distance_m({51.0, 179.9}, {51.0, -179.9}) - 14'000.0) <
           100.0);

    std::array<TrackDefinition, 3> catalog{
        make_track("alpha", 52.0, -1.0, 1'500.0),
        make_track("bravo", 52.02, -1.0, 1'000.0),
        make_track("charlie", 53.0, -1.0, 2'000.0),
    };
    const TrackCatalogView view{catalog.data(), catalog.size()};

    auto result = match_track_geofences(view, {{}, {}, false});
    assert(result.state == TrackMatchState::location_unavailable);
    assert(result.candidate_count == 0);

    result = match_track_geofences(view, {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::suggested);
    assert(result.candidate_count == 1);
    assert(result.selected_index == 0);
    assert(result.nearest_index == 0);

    result = match_track_geofences(view, {{54.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::no_match);
    assert(result.candidate_count == 0);
    assert(result.nearest_index == 2);

    std::array<TrackDefinition, 2> overlapping{
        make_track("inside", 52.0, -1.0, 1'500.0),
        make_track("adjacent", 52.005, -1.0, 1'500.0),
    };
    result = match_track_geofences({overlapping.data(), overlapping.size()},
                                   {{52.0025, -1.0}, {}, true});
    assert(result.state == TrackMatchState::ambiguous);
    assert(result.candidate_count == 2);
    assert(result.selected_index == kNoTrackIndex);
    assert(result.candidate_indices[0] == 0);
    assert(result.candidate_indices[1] == 1);

    result = match_track_geofences(view, {{}, "bravo", false});
    assert(result.state == TrackMatchState::selected);
    assert(result.selected_index == 1);
    assert(result.nearest_index == kNoTrackIndex);

    result = match_track_geofences(view, {{52.0, -1.0}, "bravo", true});
    assert(result.state == TrackMatchState::selected);
    assert(result.selected_index == 1);
    assert(result.nearest_index == 1);
    assert(result.nearest_distance_m > 2'000.0);

    result = match_track_geofences(view, {{}, "removed-track", false});
    assert(result.state == TrackMatchState::selected_track_missing);

    result = match_track_geofences({}, {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::no_match);
    assert(result.nearest_index == kNoTrackIndex);

    result = match_track_geofences({nullptr, 1}, {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::invalid_catalog);
    result = match_track_geofences({catalog.data(), kMaximumCatalogTracks + 1},
                                   {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::invalid_catalog);

    auto duplicate = overlapping;
    std::strcpy(duplicate[1].track_id.data(), "inside");
    result = match_track_geofences({duplicate.data(), duplicate.size()},
                                   {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::invalid_catalog);

    auto invalid = catalog;
    invalid[0].geofence.radius_m = -1.0;
    result = match_track_geofences({invalid.data(), invalid.size()},
                                   {{52.0, -1.0}, {}, true});
    assert(result.state == TrackMatchState::invalid_catalog);

    const GeographicPoint center{0.0, 0.0};
    const GeographicPoint boundary{0.01, 0.0};
    auto boundary_track = make_track("boundary", center.latitude_deg, center.longitude_deg,
                                     geographic_distance_m(center, boundary));
    result = match_track_geofences({&boundary_track, 1}, {boundary, {}, true});
    assert(result.state == TrackMatchState::suggested);

    for (const auto state : {TrackMatchState::location_unavailable,
                             TrackMatchState::no_match, TrackMatchState::suggested,
                             TrackMatchState::ambiguous, TrackMatchState::selected,
                             TrackMatchState::selected_track_missing,
                             TrackMatchState::invalid_catalog}) {
        assert(track_match_state_name(state)[0] != '\0');
    }

    std::cout << "Offline track geofence suggestion and ambiguity decisions passed\n";
    return 0;
}
