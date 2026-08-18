#include "track_timer/simulator/file_track_definition_store.hpp"

#include <cctype>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <system_error>

namespace track_timer::simulator {
namespace {

bool valid_id(const std::string_view id) noexcept
{
    if (id.empty() || id.size() >= track::kTrackIdCapacity) {
        return false;
    }
    for (const auto value : id) {
        if (!std::isalnum(static_cast<unsigned char>(value)) && value != '.' &&
            value != '_' && value != '-') {
            return false;
        }
    }
    return true;
}

std::filesystem::path path_for(const std::filesystem::path& directory,
                               const std::string_view id)
{
    return directory / (std::string{id} + ".json");
}

bool load_valid(const std::filesystem::path& path,
                track::TrackDefinitionBlob& output) noexcept
{
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        return false;
    }
    const auto length = input.tellg();
    if (length <= 0 || static_cast<std::uint64_t>(length) >= output.bytes.size()) {
        return false;
    }
    input.seekg(0);
    track::TrackDefinitionBlob candidate{};
    candidate.size = static_cast<std::size_t>(length);
    input.read(candidate.bytes.data(), static_cast<std::streamsize>(candidate.size));
    if (!input) {
        return false;
    }
    track::TrackDefinition parsed{};
    if (track::load_track_definition(
            {candidate.bytes.data(), candidate.size}, parsed).result !=
        track::TrackLoadResult::loaded) {
        return false;
    }
    output = candidate;
    return true;
}

bool promote(const std::filesystem::path& source,
             const std::filesystem::path& destination) noexcept
{
    std::error_code error;
    std::filesystem::remove(destination, error);
    error.clear();
    std::filesystem::rename(source, destination, error);
    return !error;
}

}  // namespace

FileTrackDefinitionStore::FileTrackDefinitionStore(std::filesystem::path directory)
    : directory_(std::move(directory))
{
}

track::TrackStoreReadResult FileTrackDefinitionStore::read(
    const std::string_view track_id, track::TrackDefinitionBlob& blob) noexcept
{
    if (!valid_id(track_id)) {
        return track::TrackStoreReadResult::invalid;
    }
    const auto path = path_for(directory_, track_id);
    std::error_code error;
    const auto path_exists = std::filesystem::exists(path, error);
    if (!error && path_exists && load_valid(path, blob)) {
        return track::TrackStoreReadResult::loaded;
    }
    const auto backup = std::filesystem::path{path.string() + ".bak"};
    track::TrackDefinitionBlob recovered{};
    if (load_valid(backup, recovered) && promote(backup, path)) {
        blob = recovered;
        return track::TrackStoreReadResult::loaded;
    }
    const auto temporary = std::filesystem::path{path.string() + ".tmp"};
    if (!path_exists && load_valid(temporary, recovered) && promote(temporary, path)) {
        blob = recovered;
        return track::TrackStoreReadResult::loaded;
    }
    if (!path_exists && !std::filesystem::exists(backup, error) &&
        !std::filesystem::exists(temporary, error)) {
        return track::TrackStoreReadResult::not_found;
    }
    return error ? track::TrackStoreReadResult::io_error
                 : track::TrackStoreReadResult::invalid;
}

bool FileTrackDefinitionStore::exists(const std::string_view track_id) noexcept
{
    if (!valid_id(track_id)) {
        return false;
    }
    std::error_code error;
    return std::filesystem::exists(path_for(directory_, track_id), error) && !error;
}

bool FileTrackDefinitionStore::write_atomic(
    const std::string_view track_id,
    const track::TrackDefinitionBlob& blob) noexcept
{
    if (!valid_id(track_id) || blob.size == 0 || blob.size >= blob.bytes.size()) {
        return false;
    }
    track::TrackDefinition parsed{};
    if (track::load_track_definition({blob.bytes.data(), blob.size}, parsed).result !=
            track::TrackLoadResult::loaded ||
        parsed.track_id.data() != track_id) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    if (error) {
        return false;
    }
    const auto path = path_for(directory_, track_id);
    const auto temporary = std::filesystem::path{path.string() + ".tmp"};
    const auto backup = std::filesystem::path{path.string() + ".bak"};
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output.write(blob.bytes.data(), static_cast<std::streamsize>(blob.size));
        output.flush();
        if (!output) {
            return false;
        }
    }
    track::TrackDefinitionBlob verified{};
    if (!load_valid(temporary, verified)) {
        return false;
    }
    std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(path, error)) {
        error.clear();
        std::filesystem::rename(path, backup, error);
        if (error) {
            return false;
        }
    }
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code restore_error;
        std::filesystem::rename(backup, path, restore_error);
        return false;
    }
    return true;
}

track::TrackStoreReadResult FileTrackDefinitionStore::list_track_ids(
    track::TrackIdList& output) noexcept
{
    output = {};
    std::error_code error;
    if (!std::filesystem::is_directory(directory_, error) || error) {
        return track::TrackStoreReadResult::not_found;
    }
    // Sorted so the catalog order is deterministic across hosts and filesystems.
    std::vector<std::string> identifiers;
    for (const auto& entry : std::filesystem::directory_iterator(directory_, error)) {
        if (error) {
            return track::TrackStoreReadResult::io_error;
        }
        if (!entry.is_regular_file(error) || error || entry.path().extension() != ".json") {
            continue;
        }
        auto stem = entry.path().stem().string();
        if (stem.empty() || stem.size() >= track::kTrackIdCapacity) {
            continue;
        }
        identifiers.push_back(std::move(stem));
    }
    std::sort(identifiers.begin(), identifiers.end());
    for (const auto& identifier : identifiers) {
        ++output.discovered;
        if (output.count >= output.ids.size()) {
            continue;
        }
        std::copy(identifier.begin(), identifier.end(), output.ids[output.count].begin());
        ++output.count;
    }
    return track::TrackStoreReadResult::loaded;
}

const std::filesystem::path& FileTrackDefinitionStore::directory() const noexcept
{
    return directory_;
}

}  // namespace track_timer::simulator
