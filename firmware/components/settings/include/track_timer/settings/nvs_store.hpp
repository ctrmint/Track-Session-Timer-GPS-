#pragma once

#include "track_timer/settings/settings.hpp"

namespace track_timer::settings {

inline constexpr const char* kNvsNamespace = "track_timer";
inline constexpr const char* kNvsSettingsKey = "settings";

// Device settings in NVS rather than on the microSD card.
//
// Settings are device identity, not portable content, and the project rule that SD
// failure must not stop timing argues against putting operating configuration somewhere
// a missing card can take away. Track geometry stays on the card; configuration does not.
//
// The blob is written whole and atomically by NVS, so a half-written settings record
// cannot be observed.
class NvsSettingsStore final : public SettingsStore {
  public:
    // Initialises the NVS partition, recovering from a full or version-mismatched
    // partition by erasing it rather than refusing to start.
    [[nodiscard]] bool begin() noexcept;

    [[nodiscard]] StoreReadResult read(SettingsBlob& blob) noexcept override;
    [[nodiscard]] bool write_atomic(const SettingsBlob& blob) noexcept override;

    [[nodiscard]] bool ready() const noexcept;

  private:
    bool ready_{false};
};

}  // namespace track_timer::settings
