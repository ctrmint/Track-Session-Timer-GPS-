#pragma once

#include "track_timer/track/definition.hpp"
#include "track_timer/track/matching.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace track_timer::track {

enum class TrackStoreReadResult : std::uint8_t {
    loaded,
    not_found,
    invalid,
    io_error,
};

class TrackDefinitionStore {
  public:
    virtual ~TrackDefinitionStore() = default;
    [[nodiscard]] virtual TrackStoreReadResult read(std::string_view track_id,
                                                    TrackDefinitionBlob& blob) noexcept = 0;
    [[nodiscard]] virtual bool exists(std::string_view track_id) noexcept = 0;
    [[nodiscard]] virtual bool write_atomic(std::string_view track_id,
                                            const TrackDefinitionBlob& blob) noexcept = 0;
};

inline constexpr std::size_t kTrackIdListCapacity = kMaximumCatalogTracks;

// `discovered` counts everything the source offered, so truncation is visible rather
// than silent: a driver must never find their circuit simply absent from the list.
struct TrackIdList {
    std::array<std::array<char, kTrackIdCapacity>, kTrackIdListCapacity> ids{};
    std::size_t count{0};
    std::size_t discovered{0};

    [[nodiscard]] constexpr bool truncated() const noexcept { return discovered > count; }
};

// Enumeration is separate from TrackDefinitionStore so that stores which cannot list
// their contents, such as in-memory test doubles, are not forced to implement it.
class TrackCatalogSource {
  public:
    virtual ~TrackCatalogSource() = default;
    [[nodiscard]] virtual TrackStoreReadResult list_track_ids(
        TrackIdList& output) noexcept = 0;
};

}  // namespace track_timer::track
