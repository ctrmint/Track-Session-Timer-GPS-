#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/track/matching.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class TrackSelectionStatus : std::uint8_t {
    closed,
    browsing,
    suggested,
    ambiguous,
    no_nearby_track,
    location_unavailable,
    selected_track_missing,
    invalid_catalog,
    saved,
    timer_only,
    storage_error,
    provisional,
    locked_active,
    capture_information,
};

struct TrackSelectionViewModel {
    std::array<char, 24> position{};
    std::array<char, 64> track_name{};
    std::array<char, 64> definition{};
    std::array<char, 96> status{};
    std::array<char, 32> select_label{};
    std::uint32_t status_color_rgb{0xFFFFFF};
    bool can_browse{false};
    bool can_select{false};
    bool can_use_timer_only{false};
};

class TrackSelectionController {
  public:
    void begin(track::TrackCatalogView catalog, const track::TrackMatchResult& match,
               const settings::DeviceSettings& current, bool session_active) noexcept;
    void previous() noexcept;
    void next() noexcept;
    [[nodiscard]] settings::SettingsApplyResult select(settings::SettingsManager& manager,
                                                        bool session_active) noexcept;
    [[nodiscard]] settings::SettingsApplyResult use_timer_only(
        settings::SettingsManager& manager, bool session_active) noexcept;
    void show_capture_information() noexcept;
    void cancel() noexcept;

    [[nodiscard]] std::size_t browse_index() const noexcept;
    [[nodiscard]] TrackSelectionStatus status() const noexcept;
    [[nodiscard]] TrackSelectionViewModel view_model() const noexcept;

  private:
    [[nodiscard]] bool catalog_usable() const noexcept;
    void set_initial_status(const track::TrackMatchResult& match,
                            const settings::DeviceSettings& current) noexcept;

    track::TrackCatalogView catalog_{};
    track::TrackMatchResult match_{};
    std::size_t browse_index_{track::kNoTrackIndex};
    TrackSelectionStatus status_{TrackSelectionStatus::closed};
};

}  // namespace track_timer::ui
