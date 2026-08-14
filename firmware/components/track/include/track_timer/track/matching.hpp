#pragma once

#include "track_timer/track/definition.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace track_timer::track {

inline constexpr std::size_t kMaximumCatalogTracks = 32;
inline constexpr std::size_t kNoTrackIndex = kMaximumCatalogTracks;

struct TrackCatalogView {
    const TrackDefinition* definitions{nullptr};
    std::size_t count{0};
};

struct TrackMatchRequest {
    GeographicPoint position{};
    std::string_view selected_track_id{};
    bool position_valid{false};
};

enum class TrackMatchState : std::uint8_t {
    location_unavailable,
    no_match,
    suggested,
    ambiguous,
    selected,
    selected_track_missing,
    invalid_catalog,
};

struct TrackMatchResult {
    TrackMatchState state{TrackMatchState::location_unavailable};
    std::array<std::size_t, kMaximumCatalogTracks> candidate_indices{};
    std::size_t candidate_count{0};
    std::size_t selected_index{kNoTrackIndex};
    std::size_t nearest_index{kNoTrackIndex};
    double nearest_distance_m{0.0};
};

[[nodiscard]] TrackMatchResult match_track_geofences(
    TrackCatalogView catalog, const TrackMatchRequest& request) noexcept;
[[nodiscard]] double geographic_distance_m(const GeographicPoint& first,
                                           const GeographicPoint& second) noexcept;
[[nodiscard]] const char* track_match_state_name(TrackMatchState state) noexcept;

static_assert(std::is_trivially_copyable_v<TrackCatalogView>);
static_assert(std::is_trivially_copyable_v<TrackMatchResult>);

}  // namespace track_timer::track
