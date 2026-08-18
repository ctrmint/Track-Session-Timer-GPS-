#pragma once

#include "track_timer/track/definition.hpp"
#include "track_timer/track/matching.hpp"
#include "track_timer/track/storage.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::catalog {

enum class CatalogBuildResult : std::uint8_t {
    built,
    source_unavailable,
    empty,
    no_storage,
};

struct CatalogStatus {
    CatalogBuildResult result{CatalogBuildResult::empty};
    std::size_t discovered{0};   // ids the source offered
    std::size_t loaded{0};       // definitions parsed and kept
    std::size_t rejected{0};     // ids that failed to read or parse
    std::size_t capacity{0};

    // True when the source held more definitions than the catalog can hold. This must
    // be surfaced to the driver, never swallowed.
    [[nodiscard]] constexpr bool truncated() const noexcept
    {
        return discovered > loaded + rejected;
    }
};

// Owns no memory. The caller supplies the definition array, which on the device is
// allocated from PSRAM: TrackDefinition is 3.6 KB, so kMaximumCatalogTracks entries is
// about 115 KB - far too much for internal RAM, negligible in external RAM.
class TrackCatalog {
  public:
    TrackCatalog(track::TrackDefinition* storage, std::size_t capacity) noexcept;

    // Re-reads every definition the source offers. Failure to parse one entry never
    // aborts the build; it is counted in `rejected` so a single corrupt file on the
    // card cannot hide every other track.
    // `scratch` is caller-owned because TrackDefinitionBlob is 16 KB, which must never
    // sit on a task stack. Allocate it once alongside the definition array.
    CatalogStatus rebuild(track::TrackCatalogSource& source,
                          track::TrackDefinitionStore& store,
                          track::TrackDefinitionBlob& scratch) noexcept;

    void clear() noexcept;

    [[nodiscard]] track::TrackCatalogView view() const noexcept;
    [[nodiscard]] const CatalogStatus& status() const noexcept;
    [[nodiscard]] std::size_t find(std::string_view track_id) const noexcept;

  private:
    track::TrackDefinition* storage_{nullptr};
    std::size_t capacity_{0};
    CatalogStatus status_{};
};

[[nodiscard]] const char* catalog_build_result_name(CatalogBuildResult result) noexcept;

}  // namespace track_timer::catalog
