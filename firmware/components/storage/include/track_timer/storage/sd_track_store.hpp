#pragma once

#include "track_timer/track/storage.hpp"

#include <cstddef>

namespace track_timer::storage {

inline constexpr const char* kTrackPackRoot = "/sdcard/track-packs";

// Captured geometry is written into its own pack so it can never be confused with a
// distributed pack, and so wiping a downloaded pack does not delete device captures.
inline constexpr const char* kCapturePackId = "device-captured";

inline constexpr std::size_t kMaximumPacks = 8;

// Track definitions on the microSD card, laid out as the pack builder emits them:
//
//   /sdcard/track-packs/<pack_id>/definitions/<track_id>.json
//
// so a pack produced by tools/build_uk_track_pack.py can be copied across unchanged.
// Multiple packs may coexist; lookups scan them in directory order.
class SdTrackStore final : public track::TrackDefinitionStore,
                           public track::TrackCatalogSource {
  public:
    explicit SdTrackStore(const char* root = kTrackPackRoot) noexcept;

    [[nodiscard]] track::TrackStoreReadResult read(
        std::string_view track_id, track::TrackDefinitionBlob& blob) noexcept override;
    [[nodiscard]] bool exists(std::string_view track_id) noexcept override;
    [[nodiscard]] bool write_atomic(std::string_view track_id,
                                    const track::TrackDefinitionBlob& blob) noexcept override;
    [[nodiscard]] track::TrackStoreReadResult list_track_ids(
        track::TrackIdList& output) noexcept override;

    [[nodiscard]] const char* root() const noexcept;

  private:
    const char* root_{kTrackPackRoot};
};

}  // namespace track_timer::storage
