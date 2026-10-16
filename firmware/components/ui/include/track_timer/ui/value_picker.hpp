#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/ui/settings_editor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

inline constexpr std::size_t kValueChoiceCapacity = 12;
inline constexpr std::size_t kValueTextCapacity = 16;

struct ValueChoice {
    std::array<char, kValueTextCapacity> text{};
};

// Every legal value for a field, presented so one press selects one value. This is what
// replaces increment/decrement stepping: reaching any value costs a single press instead
// of up to 150.
struct ValueChoiceList {
    std::array<ValueChoice, kValueChoiceCapacity> choices{};
    std::size_t count{0};
    std::size_t selected{0};  // index matching the current setting
};

// The fields the gesture UI offers. operating_mode and trackday_mode are deliberately
// absent: they are owned by the top-level Mode selection, and offering the same state in
// two places invites the two disagreeing.
// Where acceleration_target sits in kPickerFields. Setup's 0-X entry jumps straight to this
// field's values rather than walking the driver through the whole list to reach it, and a
// hand-written index that stopped matching the array is exactly the mistake that made
// DIAGNOSTICS unreachable in #158 - so it is asserted below rather than trusted.
inline constexpr std::size_t kAccelerationTargetFieldIndex = 11;

inline constexpr std::array<SettingsField, 12> kPickerFields{
    SettingsField::session_duration,   SettingsField::rest_duration,
    SettingsField::average_lap,        SettingsField::lower_display,
    SettingsField::lap_boundary,       SettingsField::pit_entry_auto_stop,
    SettingsField::launch_sensitivity, SettingsField::day_brightness,
    SettingsField::night_brightness,   SettingsField::auto_dim,
    SettingsField::orientation,       SettingsField::acceleration_target,
};

static_assert(kPickerFields[kAccelerationTargetFieldIndex] ==
                  SettingsField::acceleration_target,
              "Setup's 0-X entry opens this index, so it must be the target field");

[[nodiscard]] ValueChoiceList choices_for(SettingsField field,
                                          const settings::DeviceSettings& current) noexcept;

// Writes the chosen value into `draft`. Returns false if the index is out of range, so a
// stale screen cannot write a value the field does not have.
[[nodiscard]] bool apply_choice(SettingsField field, std::size_t index,
                                settings::DeviceSettings& draft) noexcept;

[[nodiscard]] const char* picker_field_label(SettingsField field) noexcept;

}  // namespace track_timer::ui
