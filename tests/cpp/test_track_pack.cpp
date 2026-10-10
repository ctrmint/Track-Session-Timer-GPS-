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
    std::size_t ready_count = 0;
    std::size_t provisional_count = 0;
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
        assert(identifiers.insert(definition.track_id.data()).second);

        // The two must always agree. A definition the parser calls timing-ready while the
        // engine refuses it, or the reverse, would be a track that behaves differently
        // depending on which code asked - and the driver would have no way to tell which.
        const auto ready = track_timing_ready(definition);
        const auto status = definition.provenance.geometry_status;
        assert(ready == (status != TrackGeometryStatus::provisional));

        if (ready) {
            ++ready_count;
            // Promoted by desk corroboration rather than by driving it. Physical capture
            // would read device_captured or physically_validated.
            assert(status == TrackGeometryStatus::independently_validated);
        }
        else {
            ++provisional_count;
        }
        ++loaded_count;
    }
    assert(loaded_count == 24);
    // Donington's GP and National layouts, which share one validated gate profile: the
    // circuits differ only in whether the Melbourne Loop is driven, and no gate is on it.
    //
    // This number is the whole point of #139. While it was zero, a driver could select any
    // of the 24 circuits and the timing engine would never arm, at any real venue.
    assert(ready_count == 2);
    assert(provisional_count == 22);

    std::cout << "Firmware parser accepted 24 unique UK definitions: " << ready_count
              << " timing-ready, " << provisional_count
              << " provisional and refused by the engine\n";
    return 0;
}
