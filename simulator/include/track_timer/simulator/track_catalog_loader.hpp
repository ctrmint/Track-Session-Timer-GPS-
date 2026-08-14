#pragma once

#include "track_timer/simulator/track_fixtures.hpp"

#include <filesystem>
#include <string>

namespace track_timer::simulator {

[[nodiscard]] bool append_track_catalog_directory(
    const std::filesystem::path& definitions_directory, TrackFixture& fixture,
    std::string& error);

}  // namespace track_timer::simulator
