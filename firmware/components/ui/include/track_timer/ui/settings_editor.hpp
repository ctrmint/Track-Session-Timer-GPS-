#pragma once

#include "track_timer/settings/settings.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class SettingsField : std::uint8_t {
    session_duration,
    rest_duration,
    launch_sensitivity,
    day_brightness,
    night_brightness,
    operating_mode,
    trackday_mode,
    orientation,
    auto_dim,
    average_lap,
    lower_display,
};

inline constexpr std::size_t kSettingsFieldCount = 11;

enum class SettingsEditorStatus : std::uint8_t {
    closed,
    editing,
    confirm_defaults,
    defaults_staged,
    applied,
    deferred,
    invalid,
    storage_error,
    locked_active,
    cancelled,
};

struct SettingsEditorViewModel {
    std::array<char, 32> field_name{};
    std::array<char, 32> value{};
    std::array<char, 64> status{};
    bool editing{false};
    bool confirming_defaults{false};
    bool changed{false};
};

class SettingsEditor {
  public:
    [[nodiscard]] bool begin(const settings::DeviceSettings& current,
                             bool session_active) noexcept;
    void previous_field() noexcept;
    void next_field() noexcept;
    [[nodiscard]] bool decrement() noexcept;
    [[nodiscard]] bool increment() noexcept;
    void request_restore_defaults() noexcept;
    void resolve_restore_defaults(bool confirmed) noexcept;
    void cancel() noexcept;
    [[nodiscard]] settings::SettingsApplyResult save(settings::SettingsManager& manager,
                                                      bool session_active) noexcept;

    [[nodiscard]] SettingsField field() const noexcept;
    [[nodiscard]] SettingsEditorStatus status() const noexcept;
    [[nodiscard]] const settings::DeviceSettings& draft() const noexcept;
    [[nodiscard]] bool changed() const noexcept;
    [[nodiscard]] SettingsEditorViewModel view_model() const noexcept;

  private:
    [[nodiscard]] bool can_edit() const noexcept;
    [[nodiscard]] bool adjust(bool forward) noexcept;

    settings::DeviceSettings original_{};
    settings::DeviceSettings draft_{};
    SettingsField field_{SettingsField::session_duration};
    SettingsEditorStatus status_{SettingsEditorStatus::closed};
};

[[nodiscard]] const char* settings_field_name(SettingsField field) noexcept;

}  // namespace track_timer::ui
