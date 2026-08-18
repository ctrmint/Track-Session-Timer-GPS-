#pragma once

#include "track_timer/track/storage.hpp"

#include <filesystem>

namespace track_timer::simulator {

class FileTrackDefinitionStore final : public track::TrackDefinitionStore,
                                       public track::TrackCatalogSource {
  public:
    explicit FileTrackDefinitionStore(std::filesystem::path directory);

    [[nodiscard]] track::TrackStoreReadResult read(
        std::string_view track_id, track::TrackDefinitionBlob& blob) noexcept override;
    [[nodiscard]] bool exists(std::string_view track_id) noexcept override;
    [[nodiscard]] bool write_atomic(
        std::string_view track_id,
        const track::TrackDefinitionBlob& blob) noexcept override;
    [[nodiscard]] track::TrackStoreReadResult list_track_ids(
        track::TrackIdList& output) noexcept override;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;

  private:
    std::filesystem::path directory_;
};

}  // namespace track_timer::simulator
