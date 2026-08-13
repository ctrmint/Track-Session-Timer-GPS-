#include "track_timer/track/matching.hpp"

#include <cmath>
#include <cstring>

namespace track_timer::track {
namespace {

constexpr double kEarthRadiusM = 6'371'000.0;
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;

bool valid_point(const GeographicPoint& point) noexcept
{
    return std::isfinite(point.latitude_deg) && std::isfinite(point.longitude_deg) &&
           point.latitude_deg >= -90.0 && point.latitude_deg <= 90.0 &&
           point.longitude_deg >= -180.0 && point.longitude_deg <= 180.0;
}

bool valid_definition_for_matching(const TrackDefinition& definition) noexcept
{
    bool terminated = false;
    for (const auto value : definition.track_id) {
        if (value == '\0') {
            terminated = true;
            break;
        }
    }
    return definition.schema_version == kCurrentTrackSchemaVersion &&
           definition.track_id[0] != '\0' && terminated &&
           valid_point(definition.geofence.center) &&
           std::isfinite(definition.geofence.radius_m) &&
           definition.geofence.radius_m > 0.0;
}

bool id_equal(const TrackDefinition& definition, const std::string_view identifier) noexcept
{
    std::size_t length = 0;
    while (length < definition.track_id.size() && definition.track_id[length] != '\0') {
        ++length;
    }
    return length == identifier.size() &&
           std::memcmp(definition.track_id.data(), identifier.data(), length) == 0;
}

}  // namespace

TrackMatchResult match_track_geofences(const TrackCatalogView catalog,
                                       const TrackMatchRequest& request) noexcept
{
    TrackMatchResult result{};
    result.candidate_indices.fill(kNoTrackIndex);
    if ((catalog.count > 0 && catalog.definitions == nullptr) ||
        catalog.count > kMaximumCatalogTracks) {
        result.state = TrackMatchState::invalid_catalog;
        return result;
    }

    for (std::size_t index = 0; index < catalog.count; ++index) {
        const auto& definition = catalog.definitions[index];
        if (!valid_definition_for_matching(definition)) {
            result.state = TrackMatchState::invalid_catalog;
            return result;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (id_equal(catalog.definitions[previous], definition.track_id.data())) {
                result.state = TrackMatchState::invalid_catalog;
                return result;
            }
        }
    }

    if (!request.selected_track_id.empty()) {
        for (std::size_t index = 0; index < catalog.count; ++index) {
            if (id_equal(catalog.definitions[index], request.selected_track_id)) {
                result.state = TrackMatchState::selected;
                result.selected_index = index;
                if (request.position_valid && valid_point(request.position)) {
                    result.nearest_index = index;
                    result.nearest_distance_m = geographic_distance_m(
                        request.position, catalog.definitions[index].geofence.center);
                }
                return result;
            }
        }
        result.state = TrackMatchState::selected_track_missing;
        return result;
    }

    if (!request.position_valid || !valid_point(request.position)) {
        result.state = TrackMatchState::location_unavailable;
        return result;
    }

    bool has_nearest = false;
    for (std::size_t index = 0; index < catalog.count; ++index) {
        const auto distance = geographic_distance_m(request.position,
                                                    catalog.definitions[index].geofence.center);
        if (!has_nearest || distance < result.nearest_distance_m) {
            has_nearest = true;
            result.nearest_index = index;
            result.nearest_distance_m = distance;
        }
        if (distance <= catalog.definitions[index].geofence.radius_m) {
            result.candidate_indices[result.candidate_count++] = index;
        }
    }

    if (result.candidate_count == 0) {
        result.state = TrackMatchState::no_match;
    }
    else if (result.candidate_count == 1) {
        result.state = TrackMatchState::suggested;
        result.selected_index = result.candidate_indices[0];
    }
    else {
        result.state = TrackMatchState::ambiguous;
    }
    return result;
}

double geographic_distance_m(const GeographicPoint& first,
                             const GeographicPoint& second) noexcept
{
    if (!valid_point(first) || !valid_point(second)) {
        return 0.0;
    }
    const auto latitude_1 = first.latitude_deg * kDegreesToRadians;
    const auto latitude_2 = second.latitude_deg * kDegreesToRadians;
    const auto latitude_delta = latitude_2 - latitude_1;
    const auto longitude_delta = (second.longitude_deg - first.longitude_deg) *
                                 kDegreesToRadians;
    const auto latitude_sine = std::sin(latitude_delta / 2.0);
    const auto longitude_sine = std::sin(longitude_delta / 2.0);
    const auto haversine = latitude_sine * latitude_sine + std::cos(latitude_1) *
                           std::cos(latitude_2) * longitude_sine * longitude_sine;
    return 2.0 * kEarthRadiusM *
           std::asin(std::sqrt(haversine > 1.0 ? 1.0 : haversine));
}

const char* track_match_state_name(const TrackMatchState state) noexcept
{
    switch (state) {
    case TrackMatchState::location_unavailable:
        return "location-unavailable";
    case TrackMatchState::no_match:
        return "no-match";
    case TrackMatchState::suggested:
        return "suggested";
    case TrackMatchState::ambiguous:
        return "ambiguous";
    case TrackMatchState::selected:
        return "selected";
    case TrackMatchState::selected_track_missing:
        return "selected-track-missing";
    case TrackMatchState::invalid_catalog:
        return "invalid-catalog";
    }
    return "invalid-catalog";
}

}  // namespace track_timer::track
