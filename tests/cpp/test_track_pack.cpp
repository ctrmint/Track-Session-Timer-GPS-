#include "track_timer/track/definition.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

int main(const int argc, char** argv)
{
    using namespace track_timer::track;

    assert(argc == 2);
    const auto definitions = std::filesystem::path{argv[1]} / "definitions";
    std::set<std::string> identifiers{};
    std::size_t loaded_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(definitions)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream input{entry.path(), std::ios::binary};
        const std::string json{std::istreambuf_iterator<char>{input},
                               std::istreambuf_iterator<char>{}};
        TrackDefinition definition{};
        const auto report = load_track_definition(json, definition);
        if (report.result != TrackLoadResult::loaded) {
            std::cerr << entry.path() << ": " << track_load_result_name(report.result)
                      << " at " << track_definition_field_name(report.error_field) << '\n';
        }
        assert(report.result == TrackLoadResult::loaded);
        assert(definition.provenance.geometry_status ==
               TrackGeometryStatus::provisional);
        assert(!track_timing_ready(definition));
        assert(identifiers.insert(definition.track_id.data()).second);
        ++loaded_count;
    }
    assert(loaded_count == 24);
    std::cout << "Firmware parser accepted 24 unique provisional UK definitions\n";
    return 0;
}
