#include "track_timer/catalog/track_catalog.hpp"
#include "track_timer/catalog/track_loader.hpp"
#include "track_timer/simulator/file_track_definition_store.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

namespace {

using namespace track_timer;

std::array<track::TrackDefinition, track::kMaximumCatalogTracks> definitions_storage;
// Static, not stack: the blob alone is 16 KB.
track::TrackDefinitionBlob catalog_scratch;
catalog::TrackLoadScratch load_scratch;

std::filesystem::path make_scratch(const std::string& name)
{
    auto directory = std::filesystem::temp_directory_path() / ("track-catalog-" + name);
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void write_file(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream out{path, std::ios::binary};
    out << contents;
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

settings::DeviceSettings default_settings()
{
    settings::DeviceSettings value{};
    value.session_duration_seconds = 20;
    value.rest_duration_seconds = 20;
    value.average_lap_seconds = 0;
    value.lower_display = settings::LowerDisplayMode::elapsed;
    return value;
}

// A pack directory is built from the real committed track, so the test exercises the
// same files the device will read rather than a hand-written fixture.
void catalog_builds_from_a_directory_of_definitions(const std::string& source_json)
{
    const auto directory = make_scratch("build");
    write_file(directory / "synthetic_test_loop.json", source_json);

    simulator::FileTrackDefinitionStore store{directory};
    catalog::TrackCatalog catalog{definitions_storage.data(), definitions_storage.size()};
    const auto status = catalog.rebuild(store, store, catalog_scratch);

    assert(status.result == catalog::CatalogBuildResult::built);
    assert(status.discovered == 1);
    assert(status.loaded == 1);
    assert(status.rejected == 0);
    assert(!status.truncated());
    assert(catalog.view().count == 1);
    assert(catalog.find("synthetic_test_loop") == 0);
    assert(catalog.find("not_present") == track::kNoTrackIndex);
}

// A single corrupt file must not hide every other track on the card.
void corrupt_definitions_are_counted_not_fatal(const std::string& source_json)
{
    const auto directory = make_scratch("corrupt");
    write_file(directory / "synthetic_test_loop.json", source_json);
    write_file(directory / "broken_track.json", "{ this is not json");
    write_file(directory / "empty_track.json", "");

    simulator::FileTrackDefinitionStore store{directory};
    catalog::TrackCatalog catalog{definitions_storage.data(), definitions_storage.size()};
    const auto status = catalog.rebuild(store, store, catalog_scratch);

    assert(status.result == catalog::CatalogBuildResult::built);
    assert(status.discovered == 3);
    assert(status.loaded == 1);
    assert(status.rejected == 2);
    assert(catalog.find("synthetic_test_loop") == 0);
    assert(catalog.find("broken_track") == track::kNoTrackIndex);
}

void missing_directory_reports_source_unavailable()
{
    simulator::FileTrackDefinitionStore store{
        std::filesystem::temp_directory_path() / "track-catalog-absent"};
    catalog::TrackCatalog catalog{definitions_storage.data(), definitions_storage.size()};
    const auto status = catalog.rebuild(store, store, catalog_scratch);

    assert(status.result == catalog::CatalogBuildResult::source_unavailable);
    assert(status.loaded == 0);
    assert(catalog.view().count == 0);
}

void empty_directory_reports_empty()
{
    const auto directory = make_scratch("empty");
    simulator::FileTrackDefinitionStore store{directory};
    catalog::TrackCatalog catalog{definitions_storage.data(), definitions_storage.size()};
    const auto status = catalog.rebuild(store, store, catalog_scratch);
    assert(status.result == catalog::CatalogBuildResult::empty);
    assert(status.discovered == 0);
}

std::string with_geometry_status(const std::string& source_json, const char* status)
{
    auto copy = source_json;
    const auto marker = std::string{"\"geometry_status\": \"physically_validated\""};
    const auto position = copy.find(marker);
    assert(position != std::string::npos);
    copy.replace(position, marker.size(),
                 std::string{"\"geometry_status\": \""} + status + "\"");
    return copy;
}

// Provisional geometry must stay timer-only and must never arm the timing engine, so a
// downgraded copy of the committed track has to be refused.
void provisional_geometry_is_refused_by_the_loader(const std::string& source_json)
{
    const auto directory = make_scratch("provisional");
    write_file(directory / "synthetic_test_loop.json",
               with_geometry_status(source_json, "provisional"));
    simulator::FileTrackDefinitionStore store{directory};

    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    const auto report = catalog::apply_track("synthetic_test_loop", store,
                                             default_settings(), false, engine,
                                             load_scratch, active);

    assert(report.result == catalog::TrackApplyResult::not_timing_ready);
    assert(!report.engine_configured);
    assert(!engine.configured());
    assert(report.definition_hash != 0);
    assert(std::strcmp(report.track_id.data(), "synthetic_test_loop") == 0);
}

// The committed track is already physically_validated, so it exercises the full path
// through to an armed timing engine unmodified.
void validated_geometry_configures_the_timing_engine(const std::string& source_json)
{
    const auto directory = make_scratch("validated");
    write_file(directory / "synthetic_test_loop.json", source_json);
    simulator::FileTrackDefinitionStore store{directory};

    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    const auto report = catalog::apply_track("synthetic_test_loop", store,
                                             default_settings(), false, engine,
                                             load_scratch, active);

    assert(report.result == catalog::TrackApplyResult::applied);
    assert(report.engine_configured);
    assert(engine.configured());
    assert(report.configure == timing::TimingEngineConfigureResult::configured);
    assert(std::strcmp(active.track_id.data(), "synthetic_test_loop") == 0);
    assert(report.revision == active.revision);
}

// The device store returns raw bytes and leaves validation to the parser, unlike the
// simulator store which validates on read. This double mimics the device path so the
// parse-level failure is actually covered.
class RawBlobStore final : public track::TrackDefinitionStore {
  public:
    void put(const std::string& id, const std::string& bytes) { entries_[id] = bytes; }

    track::TrackStoreReadResult read(const std::string_view track_id,
                                     track::TrackDefinitionBlob& blob) noexcept override
    {
        const auto found = entries_.find(std::string{track_id});
        if (found == entries_.end()) {
            return track::TrackStoreReadResult::not_found;
        }
        blob = {};
        std::copy(found->second.begin(), found->second.end(), blob.bytes.begin());
        blob.size = found->second.size();
        return track::TrackStoreReadResult::loaded;
    }
    bool exists(const std::string_view track_id) noexcept override
    {
        return entries_.count(std::string{track_id}) != 0;
    }
    bool write_atomic(std::string_view, const track::TrackDefinitionBlob&) noexcept override
    {
        return false;
    }

  private:
    std::map<std::string, std::string> entries_;
};

// A failed load must never disturb a track that is already live.
void a_failed_load_leaves_the_previous_track_running(const std::string& source_json)
{
    RawBlobStore store;
    store.put("synthetic_test_loop", source_json);
    store.put("broken_track", "{ not json at all");

    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    const auto first = catalog::apply_track("synthetic_test_loop", store,
                                            default_settings(), false, engine,
                                            load_scratch, active);
    assert(first.result == catalog::TrackApplyResult::applied);
    assert(engine.configured());

    // Corrupt bytes reach the parser, so this is the parse-level rejection.
    const auto broken = catalog::apply_track("broken_track", store, default_settings(),
                                             false, engine, load_scratch, active);
    assert(broken.result == catalog::TrackApplyResult::definition_invalid);
    assert(broken.load.result != track::TrackLoadResult::loaded);
    assert(!broken.engine_configured);
    // The engine and the caller's record of the live track are both untouched.
    assert(engine.configured());
    assert(std::strcmp(active.track_id.data(), "synthetic_test_loop") == 0);

    const auto absent = catalog::apply_track("no_such_track", store, default_settings(),
                                             false, engine, load_scratch, active);
    assert(absent.result == catalog::TrackApplyResult::definition_missing);
    assert(engine.configured());
    assert(std::strcmp(active.track_id.data(), "synthetic_test_loop") == 0);
}

// The same guarantee through the simulator store, which rejects corrupt files earlier.
void a_store_level_failure_also_leaves_the_previous_track_running(
    const std::string& source_json)
{
    const auto directory = make_scratch("survives-store");
    write_file(directory / "synthetic_test_loop.json", source_json);
    write_file(directory / "broken_track.json", "{ not json at all");
    simulator::FileTrackDefinitionStore store{directory};

    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    assert(catalog::apply_track("synthetic_test_loop", store, default_settings(), false,
                                engine, load_scratch, active)
               .ok());

    const auto broken = catalog::apply_track("broken_track", store, default_settings(),
                                             false, engine, load_scratch, active);
    assert(!broken.ok());
    assert(broken.result == catalog::TrackApplyResult::definition_unreadable);
    assert(engine.configured());
    assert(std::strcmp(active.track_id.data(), "synthetic_test_loop") == 0);
}

void selection_is_refused_during_an_active_session(const std::string& source_json)
{
    const auto directory = make_scratch("locked");
    write_file(directory / "synthetic_test_loop.json", source_json);
    simulator::FileTrackDefinitionStore store{directory};

    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    const auto report = catalog::apply_track("synthetic_test_loop", store,
                                             default_settings(), true, engine,
                                             load_scratch, active);
    assert(report.result == catalog::TrackApplyResult::session_active);
    assert(!engine.configured());
}

void empty_selection_is_reported_not_crashed()
{
    const auto directory = make_scratch("none");
    simulator::FileTrackDefinitionStore store{directory};
    timing::TimingEngine engine{};
    track::TrackDefinition active{};
    const auto report =
        catalog::apply_track("", store, default_settings(), false, engine,
                             load_scratch, active);
    assert(report.result == catalog::TrackApplyResult::no_selection);
    assert(!engine.configured());
}

}  // namespace

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto source_json = read_file(argv[1]);
    assert(!source_json.empty());

    catalog_builds_from_a_directory_of_definitions(source_json);
    corrupt_definitions_are_counted_not_fatal(source_json);
    missing_directory_reports_source_unavailable();
    empty_directory_reports_empty();
    provisional_geometry_is_refused_by_the_loader(source_json);
    validated_geometry_configures_the_timing_engine(source_json);
    a_failed_load_leaves_the_previous_track_running(source_json);
    a_store_level_failure_also_leaves_the_previous_track_running(source_json);
    selection_is_refused_during_an_active_session(source_json);
    empty_selection_is_reported_not_crashed();

    std::cout << "Card-backed catalog build, corrupt-entry tolerance, and timing-engine "
                 "load/rollback passed\n";
    return 0;
}
