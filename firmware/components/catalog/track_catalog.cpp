#include "track_timer/catalog/track_catalog.hpp"

#include <cstring>

namespace track_timer::catalog {
namespace {

[[nodiscard]] std::string_view id_view(const std::array<char, track::kTrackIdCapacity>& id) noexcept
{
    const auto length = ::strnlen(id.data(), id.size());
    return {id.data(), length};
}

}  // namespace

TrackCatalog::TrackCatalog(track::TrackDefinition* const storage,
                           const std::size_t capacity) noexcept
    : storage_(storage), capacity_(storage == nullptr ? 0 : capacity)
{
    status_.capacity = capacity_;
}

CatalogStatus TrackCatalog::rebuild(track::TrackCatalogSource& source,
                                    track::TrackDefinitionStore& store,
                                    track::TrackDefinitionBlob& scratch) noexcept
{
    clear();
    if (storage_ == nullptr || capacity_ == 0) {
        status_.result = CatalogBuildResult::no_storage;
        return status_;
    }

    // 1.5 KB, which is acceptable on a stack; the 16 KB blob is not, hence `scratch`.
    track::TrackIdList listing{};
    if (source.list_track_ids(listing) != track::TrackStoreReadResult::loaded) {
        status_.result = CatalogBuildResult::source_unavailable;
        return status_;
    }
    status_.discovered = listing.discovered;

    for (std::size_t index = 0; index < listing.count && status_.loaded < capacity_;
         ++index) {
        const auto identifier = id_view(listing.ids[index]);
        if (identifier.empty()) {
            ++status_.rejected;
            continue;
        }

        scratch = {};
        if (store.read(identifier, scratch) != track::TrackStoreReadResult::loaded) {
            ++status_.rejected;
            continue;
        }

        auto& slot = storage_[status_.loaded];
        const auto report =
            track::load_track_definition({scratch.bytes.data(), scratch.size}, slot);
        if (report.result != track::TrackLoadResult::loaded) {
            slot = {};
            ++status_.rejected;
            continue;
        }
        ++status_.loaded;
    }

    status_.result = status_.loaded == 0 ? CatalogBuildResult::empty
                                          : CatalogBuildResult::built;
    return status_;
}

void TrackCatalog::clear() noexcept
{
    for (std::size_t index = 0; index < status_.loaded; ++index) {
        storage_[index] = {};
    }
    status_ = {};
    status_.capacity = capacity_;
}

track::TrackCatalogView TrackCatalog::view() const noexcept
{
    return {storage_, status_.loaded};
}

const CatalogStatus& TrackCatalog::status() const noexcept { return status_; }

std::size_t TrackCatalog::find(const std::string_view track_id) const noexcept
{
    if (storage_ == nullptr || track_id.empty()) {
        return track::kNoTrackIndex;
    }
    for (std::size_t index = 0; index < status_.loaded; ++index) {
        if (id_view(storage_[index].track_id) == track_id) {
            return index;
        }
    }
    return track::kNoTrackIndex;
}

const char* catalog_build_result_name(const CatalogBuildResult result) noexcept
{
    switch (result) {
    case CatalogBuildResult::built:
        return "built";
    case CatalogBuildResult::source_unavailable:
        return "source-unavailable";
    case CatalogBuildResult::empty:
        return "empty";
    case CatalogBuildResult::no_storage:
        return "no-storage";
    }
    return "unknown";
}

}  // namespace track_timer::catalog
