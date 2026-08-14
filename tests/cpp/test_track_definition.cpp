#include "track_timer/track/definition.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

std::string read_file(const char* path)
{
    std::ifstream input{path, std::ios::binary};
    assert(input);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string replace_once(std::string value, const std::string& from,
                         const std::string& to)
{
    const auto position = value.find(from);
    assert(position != std::string::npos);
    value.replace(position, from.size(), to);
    return value;
}

std::string replace_once_after(std::string value, const std::string& marker,
                               const std::string& from, const std::string& to)
{
    const auto marker_position = value.find(marker);
    assert(marker_position != std::string::npos);
    const auto position = value.find(from, marker_position + marker.size());
    assert(position != std::string::npos);
    value.replace(position, from.size(), to);
    return value;
}

void assert_failure_preserves_active(const std::string& json,
                                     const track_timer::track::TrackLoadResult expected)
{
    track_timer::track::TrackDefinition active{};
    std::strcpy(active.track_id.data(), "active-definition");
    active.definition_hash = 42;
    const auto report = track_timer::track::load_track_definition(json, active);
    assert(report.result == expected);
    assert(std::strcmp(active.track_id.data(), "active-definition") == 0);
    assert(active.definition_hash == 42);
}

}  // namespace

int main(const int argc, char** argv)
{
    using namespace track_timer::track;

    assert(argc == 2);
    const auto valid_json = read_file(argv[1]);
    assert(valid_json.size() <= kMaximumTrackFileBytes);
    TrackDefinition definition{};
    const auto loaded = load_track_definition(valid_json, definition);
    assert(loaded.result == TrackLoadResult::loaded);
    assert(loaded.error_offset == valid_json.size());
    assert(definition.schema_version == kCurrentTrackSchemaVersion);
    assert(std::strcmp(definition.track_id.data(), "synthetic_test_loop") == 0);
    assert(std::strcmp(definition.name.data(), "Synthetic Test Loop") == 0);
    assert(std::strcmp(definition.country.data(), "XX") == 0);
    assert(definition.sector_count == 0);
    assert(std::abs(definition.gates.start.local_left.north_m - 5.5597) < 0.1);
    assert(std::abs(definition.gates.start.local_right.north_m + 5.5597) < 0.1);
    assert(std::hypot(definition.gates.start.local_right.east_m -
                          definition.gates.start.local_left.east_m,
                      definition.gates.start.local_right.north_m -
                          definition.gates.start.local_left.north_m) > 10.0);
    assert(definition.gates.start.left.latitude_deg ==
           definition.gates.finish.left.latitude_deg);
    assert(definition.gates.start.right.longitude_deg ==
           definition.gates.finish.right.longitude_deg);
    assert(std::hypot(definition.gates.pit_entry.local_right.east_m -
                          definition.gates.pit_entry.local_left.east_m,
                      definition.gates.pit_entry.local_right.north_m -
                          definition.gates.pit_entry.local_left.north_m) > 10.0);
    assert(std::hypot(definition.gates.pit_exit.local_right.east_m -
                          definition.gates.pit_exit.local_left.east_m,
                      definition.gates.pit_exit.local_right.north_m -
                          definition.gates.pit_exit.local_left.north_m) > 10.0);
    assert(definition.minimum_lap_time_s == 20.0);
    assert(definition.definition_hash == hash_track_definition(valid_json));
    std::array<char, 17> hash_text{};
    format_definition_hash(definition.definition_hash, hash_text);
    assert(std::strlen(hash_text.data()) == 16);

    TrackDefinition loaded_again{};
    assert(load_track_definition(valid_json, loaded_again).result == TrackLoadResult::loaded);
    assert(loaded_again.definition_hash == definition.definition_hash);
    const auto changed_bytes = valid_json + " ";
    TrackDefinition changed{};
    assert(load_track_definition(changed_bytes, changed).result == TrackLoadResult::loaded);
    assert(changed.definition_hash != definition.definition_hash);

    assert_failure_preserves_active("", TrackLoadResult::empty);
    assert_failure_preserves_active(std::string(kMaximumTrackFileBytes + 1, ' '),
                                    TrackLoadResult::file_too_large);
    assert_failure_preserves_active("{", TrackLoadResult::invalid_json);
    assert_failure_preserves_active(replace_once(valid_json, "\"schema_version\": 2",
                                                 "\"schema_version\": 1"),
                                    TrackLoadResult::unsupported_version);
    assert_failure_preserves_active(replace_once(valid_json, "\"name\": \"Synthetic Test Loop\",\n",
                                                 ""),
                                    TrackLoadResult::missing_required_field);
    assert_failure_preserves_active(replace_once(valid_json, "\"lat_deg\": 52.0",
                                                 "\"lat_deg\": 92.0"),
                                    TrackLoadResult::invalid_value);
    assert_failure_preserves_active(replace_once(valid_json, "\"lat_deg\": 52.0",
                                                 "\"lat_deg\": 86.0"),
                                    TrackLoadResult::invalid_value);
    assert_failure_preserves_active(replace_once(valid_json, "\"name\": \"Synthetic Test Loop\"",
                                                 "\"name\": \"" + std::string(80, 'A') + "\""),
                                    TrackLoadResult::capacity_exceeded);
    assert_failure_preserves_active(
        replace_once_after(valid_json, "\"start\"",
                           "\"lat_deg\": 51.99995,\n        \"lon_deg\": -0.99998",
                           "\"lat_deg\": 52.00005,\n        \"lon_deg\": -1.00002"),
        TrackLoadResult::degenerate_start_gate);
    assert_failure_preserves_active(
        replace_once_after(valid_json, "\"finish\"",
                           "\"lat_deg\": 51.99995,\n        \"lon_deg\": -0.99998",
                           "\"lat_deg\": 52.00005,\n        \"lon_deg\": -1.00002"),
        TrackLoadResult::degenerate_finish_gate);
    assert_failure_preserves_active(
        replace_once_after(valid_json, "\"pit_entry\"",
                           "\"lat_deg\": 52.00005,\n        \"lon_deg\": -1.0",
                           "\"lat_deg\": 52.00015,\n        \"lon_deg\": -1.00004"),
        TrackLoadResult::degenerate_pit_entry_gate);
    assert_failure_preserves_active(
        replace_once_after(valid_json, "\"pit_exit\"",
                           "\"lat_deg\": 51.99985,\n        \"lon_deg\": -0.99996",
                           "\"lat_deg\": 51.99995,\n        \"lon_deg\": -1.0"),
        TrackLoadResult::degenerate_pit_exit_gate);
    assert_failure_preserves_active(
        replace_once(valid_json, "\"minimum_crossing_speed_mps\": 2.0",
                     "\"minimum_crossing_speed_mps\": 0.0"),
        TrackLoadResult::invalid_value);
    assert_failure_preserves_active(
        replace_once(valid_json, "\"rearm_corridor_m\": 15.0",
                     "\"rearm_corridor_m\": 1001.0"),
        TrackLoadResult::invalid_value);
    assert_failure_preserves_active(
        replace_once_after(valid_json, "\"start\"", "\"left\": {",
                           "\"left\": {\"unknown_geometry\": 1,"),
        TrackLoadResult::invalid_json);
    assert_failure_preserves_active(
        replace_once(valid_json, "\"pit_exit\": {", "\"pit_entry\": {"),
        TrackLoadResult::invalid_json);

    const std::string with_unknown = replace_once(
        valid_json, "\"country\": \"XX\",",
        "\"country\": \"XX\", \"provenance\": {\"source\": \"test\", \"year\": 2026},");
    TrackDefinition compatible{};
    assert(load_track_definition(with_unknown, compatible).result == TrackLoadResult::loaded);

    std::string too_many_sectors = valid_json;
    const std::string sector_entries =
        "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}";
    too_many_sectors = replace_once(too_many_sectors, "\"sectors\": []",
                                    "\"sectors\": [" + sector_entries + "]");
    assert_failure_preserves_active(too_many_sectors, TrackLoadResult::capacity_exceeded);

    for (const auto result : {TrackLoadResult::loaded, TrackLoadResult::empty,
                              TrackLoadResult::file_too_large, TrackLoadResult::invalid_json,
                              TrackLoadResult::missing_required_field,
                              TrackLoadResult::unsupported_version,
                              TrackLoadResult::invalid_value,
                              TrackLoadResult::capacity_exceeded,
                              TrackLoadResult::degenerate_start_gate,
                              TrackLoadResult::degenerate_finish_gate,
                              TrackLoadResult::degenerate_pit_entry_gate,
                              TrackLoadResult::degenerate_pit_exit_gate}) {
        assert(track_load_result_name(result)[0] != '\0');
    }

    std::cout << "Versioned track loading, projection, hashing, and failure isolation passed\n";
    return 0;
}
