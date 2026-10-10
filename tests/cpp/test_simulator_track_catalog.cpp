#include "track_timer/simulator/track_catalog_loader.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/track/definition.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace {

bool contains_track(const track_timer::simulator::TrackFixture& fixture,
                    const char* identifier)
{
    for (std::size_t index = 0; index < fixture.count; ++index) {
        if (std::strcmp(fixture.definitions[index].track_id.data(), identifier) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(const int argc, char** argv)
{
    using namespace track_timer;

    assert(argc == 2);
    auto fixture = simulator::make_track_fixture(simulator::TrackFixtureId::suggested);
    assert(fixture.count == 2);

    std::string error;
    assert(simulator::append_track_catalog_directory(argv[1], fixture, error));
    assert(error.empty());
    assert(fixture.count == 26);
    assert(std::strcmp(fixture.definitions[0].track_id.data(),
                       "synthetic_test_loop") == 0);
    assert(std::strcmp(fixture.definitions[1].track_id.data(),
                       "adjacent_test_loop") == 0);
    assert(std::strcmp(fixture.definitions[2].track_id.data(),
                       "gb_brands_hatch_gp") == 0);
    assert(std::strcmp(fixture.definitions[2].name.data(), "Brands Hatch GP") == 0);

    std::set<std::string> identifiers{};
    for (std::size_t index = 0; index < fixture.count; ++index) {
        assert(identifiers.insert(fixture.definitions[index].track_id.data()).second);
    }
    // The loader used to refuse any definition that was not provisional, which was a
    // reasonable guard while nothing in the pack could ever be promoted. It is wrong now:
    // it would have kept the simulator from ever exercising a circuit that can actually
    // arm, which is the one thing worth exercising. The pack builder is what enforces
    // that a promotion carries evidence; the loader only has to agree with the engine.
    std::size_t ready_count = 0;
    for (std::size_t index = 2; index < fixture.count; ++index) {
        const auto& definition = fixture.definitions[index];
        const auto ready = track::track_timing_ready(definition);
        assert(ready == (definition.provenance.geometry_status !=
                         track::TrackGeometryStatus::provisional));
        if (ready) {
            ++ready_count;
        }
        if (index > 2) {
            assert(std::strcmp(fixture.definitions[index - 1].track_id.data(),
                               definition.track_id.data()) < 0);
        }
    }
    // Donington's two layouts: the first real circuits in the pack that can arm timing.
    assert(ready_count == 2);
    assert(contains_track(fixture, "gb_donington_gp"));
    assert(contains_track(fixture, "gb_donington_national"));
    assert(contains_track(fixture, "gb_brands_hatch_gp"));
    assert(contains_track(fixture, "gb_knockhill_international_cw"));
    assert(contains_track(fixture, "gb_silverstone_gp"));

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto temporary = std::filesystem::temp_directory_path() /
                           ("track-timer-catalog-test-" + unique_suffix);
    std::filesystem::create_directories(temporary);
    {
        std::ofstream malformed{temporary / "malformed.json"};
        malformed << "{not-json}\n";
    }
    auto unchanged = simulator::make_track_fixture(simulator::TrackFixtureId::suggested);
    assert(!simulator::append_track_catalog_directory(temporary, unchanged, error));
    assert(error.find("invalid track definition") != std::string::npos);
    assert(unchanged.count == 2);
    assert(std::strcmp(unchanged.definitions[0].track_id.data(),
                       "synthetic_test_loop") == 0);
    std::filesystem::remove_all(temporary);

    const auto missing = temporary / "missing";
    assert(!simulator::append_track_catalog_directory(missing, unchanged, error));
    assert(error.find("directory is missing") != std::string::npos);
    assert(unchanged.count == 2);

    std::cout << "Simulator loaded 24 deterministic UK tracks safely: 2 can arm timing, 22 are timer-only\n";
    return 0;
}
