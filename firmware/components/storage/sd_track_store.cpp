#include "track_timer/storage/sd_track_store.hpp"

#include "track_timer/storage/sd_card.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace track_timer::storage {
namespace {

constexpr const char* kDefinitionsDir = "definitions";
constexpr const char* kJsonSuffix = ".json";

[[nodiscard]] bool valid_track_id(const std::string_view id) noexcept
{
    // Also a path-traversal guard: the identifier becomes a filename, so anything
    // outside the schema's own character set is refused rather than sanitised.
    if (id.empty() || id.size() >= track::kTrackIdCapacity) {
        return false;
    }
    for (const char character : id) {
        const auto value = static_cast<unsigned char>(character);
        const bool allowed = (value >= '0' && value <= '9') ||
                             (value >= 'A' && value <= 'Z') ||
                             (value >= 'a' && value <= 'z') || character == '-' ||
                             character == '_' || character == '.';
        if (!allowed) {
            return false;
        }
    }
    return id.find("..") == std::string_view::npos;
}

// Builds "<root>/<pack>/definitions/<id>.json" with every component length-bounded so
// the compiler can prove no truncation.
void definition_path(char* out, const std::size_t size, const char* root,
                     const char* pack, const std::string_view id) noexcept
{
    char identifier[track::kTrackIdCapacity]{};
    const auto copied = id.size() < sizeof(identifier) - 1 ? id.size()
                                                            : sizeof(identifier) - 1;
    std::memcpy(identifier, id.data(), copied);
    std::snprintf(out, size, "%.48s/%.48s/%s/%.47s%s", root, pack, kDefinitionsDir,
                  identifier, kJsonSuffix);
}

[[nodiscard]] bool read_whole_file(const char* path,
                                   track::TrackDefinitionBlob& blob) noexcept
{
    struct stat details {};
    if (stat(path, &details) != 0 || !S_ISREG(details.st_mode)) {
        return false;
    }
    if (details.st_size <= 0 ||
        static_cast<std::size_t>(details.st_size) > blob.bytes.size()) {
        return false;
    }
    auto* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    const auto wanted = static_cast<std::size_t>(details.st_size);
    const auto read_bytes = std::fread(blob.bytes.data(), 1, wanted, file);
    std::fclose(file);
    if (read_bytes != wanted) {
        return false;
    }
    blob.size = read_bytes;
    return true;
}

}  // namespace

SdTrackStore::SdTrackStore(const char* const root) noexcept
    : root_(root == nullptr ? kTrackPackRoot : root)
{
}

track::TrackStoreReadResult SdTrackStore::read(const std::string_view track_id,
                                                track::TrackDefinitionBlob& blob) noexcept
{
    blob = {};
    if (!valid_track_id(track_id)) {
        return track::TrackStoreReadResult::invalid;
    }
    if (!mounted()) {
        return track::TrackStoreReadResult::io_error;
    }

    auto* packs = opendir(root_);
    if (packs == nullptr) {
        return track::TrackStoreReadResult::io_error;
    }
    auto outcome = track::TrackStoreReadResult::not_found;
    while (const auto* pack = readdir(packs)) {
        if (pack->d_name[0] == '.') {
            continue;
        }
        char path[256]{};
        definition_path(path, sizeof(path), root_, pack->d_name, track_id);
        struct stat details {};
        if (stat(path, &details) != 0) {
            continue;
        }
        outcome = read_whole_file(path, blob) ? track::TrackStoreReadResult::loaded
                                              : track::TrackStoreReadResult::invalid;
        break;
    }
    closedir(packs);
    return outcome;
}

bool SdTrackStore::exists(const std::string_view track_id) noexcept
{
    if (!valid_track_id(track_id) || !mounted()) {
        return false;
    }
    auto* packs = opendir(root_);
    if (packs == nullptr) {
        return false;
    }
    bool found = false;
    while (const auto* pack = readdir(packs)) {
        if (pack->d_name[0] == '.') {
            continue;
        }
        char path[256]{};
        definition_path(path, sizeof(path), root_, pack->d_name, track_id);
        struct stat details {};
        if (stat(path, &details) == 0 && S_ISREG(details.st_mode)) {
            found = true;
            break;
        }
    }
    closedir(packs);
    return found;
}

bool SdTrackStore::write_atomic(const std::string_view track_id,
                                const track::TrackDefinitionBlob& blob) noexcept
{
    if (!valid_track_id(track_id) || blob.size == 0 || blob.size > blob.bytes.size() ||
        !mounted()) {
        return false;
    }

    char pack_dir[160]{};
    std::snprintf(pack_dir, sizeof(pack_dir), "%.48s/%.48s", root_, kCapturePackId);
    (void)mkdir(root_, 0777);
    (void)mkdir(pack_dir, 0777);
    char definitions_dir[224]{};
    std::snprintf(definitions_dir, sizeof(definitions_dir), "%.160s/%s", pack_dir,
                  kDefinitionsDir);
    (void)mkdir(definitions_dir, 0777);

    char final_path[256]{};
    definition_path(final_path, sizeof(final_path), root_, kCapturePackId, track_id);
    char temporary_path[264]{};
    std::snprintf(temporary_path, sizeof(temporary_path), "%.250s.tmp", final_path);

    auto* file = std::fopen(temporary_path, "wb");
    if (file == nullptr) {
        return false;
    }
    const auto written = std::fwrite(blob.bytes.data(), 1, blob.size, file);
    const bool flushed = std::fflush(file) == 0;
    std::fclose(file);
    if (written != blob.size || !flushed) {
        (void)remove(temporary_path);
        return false;
    }
    // rename() over an existing file is not guaranteed on FATFS, so clear the target.
    (void)remove(final_path);
    if (rename(temporary_path, final_path) != 0) {
        (void)remove(temporary_path);
        return false;
    }
    return true;
}

track::TrackStoreReadResult SdTrackStore::list_track_ids(
    track::TrackIdList& output) noexcept
{
    output = {};
    if (!mounted()) {
        return track::TrackStoreReadResult::io_error;
    }
    auto* packs = opendir(root_);
    if (packs == nullptr) {
        return track::TrackStoreReadResult::not_found;
    }

    std::size_t packs_seen = 0;
    while (const auto* pack = readdir(packs)) {
        if (pack->d_name[0] == '.' || ++packs_seen > kMaximumPacks) {
            continue;
        }
        char definitions[224]{};
        std::snprintf(definitions, sizeof(definitions), "%.48s/%.48s/%s", root_,
                      pack->d_name, kDefinitionsDir);
        auto* entries = opendir(definitions);
        if (entries == nullptr) {
            continue;
        }
        while (const auto* entry = readdir(entries)) {
            const auto length = std::strlen(entry->d_name);
            const auto suffix = std::strlen(kJsonSuffix);
            if (entry->d_name[0] == '.' || length <= suffix ||
                std::strcmp(entry->d_name + length - suffix, kJsonSuffix) != 0) {
                continue;
            }
            const std::string_view identifier{entry->d_name, length - suffix};
            if (!valid_track_id(identifier)) {
                continue;
            }
            // Counted whether or not it fits, so truncation is reported rather than
            // silently dropping circuits from the driver's list.
            ++output.discovered;
            if (output.count >= output.ids.size()) {
                continue;
            }
            std::memcpy(output.ids[output.count].data(), identifier.data(),
                        identifier.size());
            ++output.count;
        }
        closedir(entries);
    }
    closedir(packs);
    return track::TrackStoreReadResult::loaded;
}

const char* SdTrackStore::root() const noexcept { return root_; }

}  // namespace track_timer::storage
