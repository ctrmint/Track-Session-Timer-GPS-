#pragma once

#include "track_timer/track/matching.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace track_timer::simulator {

enum class TrackFixtureId : std::uint8_t {
    selected,
    missing,
    invalid,
    ambiguous,
    suggested,
    none,
    unavailable,
};

struct TrackFixture {
    std::array<track::TrackDefinition, 3> definitions{};
    std::size_t count{0};
    track::TrackMatchRequest request{};

    [[nodiscard]] track::TrackCatalogView catalog() const noexcept;
};

[[nodiscard]] TrackFixture make_track_fixture(TrackFixtureId id) noexcept;
[[nodiscard]] bool parse_track_fixture(std::string_view text,
                                       TrackFixtureId& id) noexcept;
[[nodiscard]] const char* track_fixture_name(TrackFixtureId id) noexcept;

}  // namespace track_timer::simulator
