#include "track_timer/simulator/track_catalog_loader.hpp"

#include "track_timer/track/definition.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace track_timer::simulator {
namespace {

bool same_track_id(const track::TrackDefinition& first,
                   const track::TrackDefinition& second) noexcept
{
    return std::strcmp(first.track_id.data(), second.track_id.data()) == 0;
}

}  // namespace

bool append_track_catalog_directory(const std::filesystem::path& definitions_directory,
                                    TrackFixture& fixture, std::string& error)
{
    error.clear();
    try {
        if (!std::filesystem::is_directory(definitions_directory)) {
            error = "track definition directory is missing: " +
                    definitions_directory.string();
            return false;
        }

        std::vector<std::filesystem::path> paths{};
        for (const auto& entry :
             std::filesystem::directory_iterator(definitions_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());
        if (paths.empty()) {
            error = "track definition directory contains no JSON files: " +
                    definitions_directory.string();
            return false;
        }
        if (fixture.count > fixture.definitions.size() ||
            paths.size() > fixture.definitions.size() - fixture.count) {
            error = "track catalog exceeds the bounded device capacity";
            return false;
        }

        auto candidate = fixture;
        for (const auto& path : paths) {
            if (std::filesystem::file_size(path) > track::kMaximumTrackFileBytes) {
                error = "track definition exceeds the file-size limit: " + path.string();
                return false;
            }
            std::ifstream input{path, std::ios::binary};
            if (!input.is_open()) {
                error = "cannot open track definition: " + path.string();
                return false;
            }
            const std::string json{std::istreambuf_iterator<char>{input},
                                   std::istreambuf_iterator<char>{}};
            if (input.bad()) {
                error = "cannot read track definition: " + path.string();
                return false;
            }

            track::TrackDefinition definition{};
            const auto report = track::load_track_definition(json, definition);
            if (report.result != track::TrackLoadResult::loaded) {
                error = "invalid track definition " + path.string() + ": " +
                        track::track_load_result_name(report.result);
                return false;
            }
            for (std::size_t index = 0; index < candidate.count; ++index) {
                if (same_track_id(candidate.definitions[index], definition)) {
                    error = "duplicate track identifier: " +
                            std::string{definition.track_id.data()};
                    return false;
                }
            }
            candidate.definitions[candidate.count++] = definition;
        }

        fixture = candidate;
        return true;
    }
    catch (const std::filesystem::filesystem_error& exception) {
        error = "cannot enumerate track catalog: " + std::string{exception.what()};
        return false;
    }
}

}  // namespace track_timer::simulator
