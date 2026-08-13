#pragma once

#include "track_timer/settings/settings.hpp"

#include <filesystem>

namespace track_timer::simulator {

class FileSettingsStore final : public settings::SettingsStore {
  public:
    explicit FileSettingsStore(std::filesystem::path path);

    settings::StoreReadResult read(settings::SettingsBlob& blob) noexcept override;
    bool write_atomic(const settings::SettingsBlob& blob) noexcept override;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;

  private:
    std::filesystem::path path_;
};

}  // namespace track_timer::simulator
