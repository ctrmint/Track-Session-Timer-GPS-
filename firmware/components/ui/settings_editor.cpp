#include "track_timer/ui/settings_editor.hpp"

#include "track_timer/ui/time_roller.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

// Seconds now, since the durations are stored in seconds. The stepping editor keeps its
// coarse preset ladder; the roller in the gated menu is what reaches every value.
constexpr std::array<std::uint32_t, 10> kDurationSeconds{60,   300,  600,  900,  1200,
                                                        1500, 1800, 2400, 3000, 3600};
constexpr std::array<std::uint8_t, 4> kBrightnessPercent{25, 50, 75, 100};

template <typename T, std::size_t Size>
bool step_choice(T& value, const std::array<T, Size>& choices, const bool forward) noexcept
{
    const auto found = std::find(choices.begin(), choices.end(), value);
    if (found == choices.end()) {
        if (forward) {
            const auto next = std::upper_bound(choices.begin(), choices.end(), value);
            if (next == choices.end()) {
                return false;
            }
            value = *next;
        }
        else {
            const auto next = std::lower_bound(choices.begin(), choices.end(), value);
            if (next == choices.begin()) {
                return false;
            }
            value = *(next - 1);
        }
        return true;
    }
    const auto index = static_cast<std::size_t>(found - choices.begin());
    const auto next = forward ? std::min(index + 1, Size - 1) : (index == 0 ? 0 : index - 1);
    if (next == index) {
        return false;
    }
    value = choices[next];
    return true;
}

template <typename Enum>
bool step_enum(Enum& value, const std::uint8_t maximum, const bool forward) noexcept
{
    auto numeric = static_cast<std::uint8_t>(value);
    numeric = forward ? static_cast<std::uint8_t>((numeric + 1U) % (maximum + 1U))
                      : static_cast<std::uint8_t>(numeric == 0 ? maximum : numeric - 1U);
    value = static_cast<Enum>(numeric);
    return true;
}

const char* status_text(const SettingsEditorStatus status) noexcept
{
    switch (status) {
    case SettingsEditorStatus::closed:
        return "SETTINGS CLOSED";
    case SettingsEditorStatus::editing:
        return "SELECT A FIELD AND ADJUST ITS VALUE";
    case SettingsEditorStatus::confirm_defaults:
        return "RESTORE ALL DEFAULTS? CONFIRM OR KEEP CHANGES";
    case SettingsEditorStatus::defaults_staged:
        return "DEFAULTS STAGED — SELECT SAVE TO APPLY";
    case SettingsEditorStatus::applied:
        return "SAVED";
    case SettingsEditorStatus::deferred:
        return "SAVED FOR THE NEXT READY STATE";
    case SettingsEditorStatus::invalid:
        return "INVALID COMBINATION — CORRECT THE VALUE";
    case SettingsEditorStatus::storage_error:
        return "SAVE FAILED — SETTINGS WERE NOT CHANGED";
    case SettingsEditorStatus::locked_active:
        return "SETTINGS LOCKED WHILE A SESSION IS ACTIVE";
    case SettingsEditorStatus::cancelled:
        return "CHANGES DISCARDED";
    }
    return "SETTINGS";
}

void format_value(const SettingsField field, const settings::DeviceSettings& settings,
                  std::array<char, 32>& output) noexcept
{
    switch (field) {
    case SettingsField::session_duration:
        format_duration_value(output.data(), output.size(), settings.session_duration_seconds);
        break;
    case SettingsField::rest_duration:
        format_duration_value(output.data(), output.size(), settings.rest_duration_seconds);
        break;
    case SettingsField::launch_sensitivity:
        // Always a real threshold now; TRIGGER decides whether it is consulted.
        std::snprintf(output.data(), output.size(), "%u.%02u g",
                      static_cast<unsigned>(settings.launch_sensitivity_milli_g / 1000),
                      static_cast<unsigned>((settings.launch_sensitivity_milli_g % 1000) / 10));
        break;
    case SettingsField::day_brightness:
        std::snprintf(output.data(), output.size(), "%u%%",
                      static_cast<unsigned>(settings.day_brightness_percent));
        break;
    case SettingsField::night_brightness:
        std::snprintf(output.data(), output.size(), "%u%%",
                      static_cast<unsigned>(settings.night_brightness_percent));
        break;
    case SettingsField::operating_mode:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.operating_mode == settings::OperatingMode::timer ? "TIMER"
                                                                               : "G METER");
        break;
    case SettingsField::trackday_mode:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.trackday_mode_enabled ? "ENABLED" : "DISABLED");
        break;
    case SettingsField::lap_boundary:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.lap_boundary == settings::LapBoundaryMode::start
                          ? "START LINE"
                          : "FINISH LINE");
        break;
    case SettingsField::pit_exit_auto_start:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.pit_exit_auto_start_enabled ? "ENABLED" : "DISABLED");
        break;
    case SettingsField::pit_entry_auto_stop:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.pit_entry_auto_stop_enabled ? "ENABLED" : "DISABLED");
        break;
    case SettingsField::orientation: {
        constexpr std::array<const char*, 5> names{"0 DEG", "90 DEG", "180 DEG", "270 DEG",
                                                   "AUTO"};
        std::snprintf(output.data(), output.size(), "%s",
                      names[static_cast<std::size_t>(settings.orientation)]);
        break;
    }
    case SettingsField::auto_dim:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.auto_dim_enabled ? "ON" : "OFF");
        break;
    case SettingsField::average_lap:
        std::snprintf(output.data(), output.size(), "%02u:%02u",
                      static_cast<unsigned>(settings.average_lap_seconds / 60),
                      static_cast<unsigned>(settings.average_lap_seconds % 60));
        break;
    case SettingsField::lower_display:
        std::snprintf(output.data(), output.size(), "%s",
                      settings.lower_display == settings::LowerDisplayMode::elapsed
                          ? "COUNT UP"
                          : "LAPS LEFT");
        break;
    }
}

}  // namespace

bool SettingsEditor::begin(const settings::DeviceSettings& current,
                           const bool session_active) noexcept
{
    original_ = current;
    draft_ = current;
    field_ = SettingsField::session_duration;
    status_ = session_active ? SettingsEditorStatus::locked_active
                             : SettingsEditorStatus::editing;
    return !session_active;
}

void SettingsEditor::previous_field() noexcept
{
    if (!can_edit()) {
        return;
    }
    const auto index = static_cast<std::size_t>(field_);
    field_ = static_cast<SettingsField>(index == 0 ? kSettingsFieldCount - 1 : index - 1);
    status_ = SettingsEditorStatus::editing;
}

void SettingsEditor::next_field() noexcept
{
    if (!can_edit()) {
        return;
    }
    field_ = static_cast<SettingsField>((static_cast<std::size_t>(field_) + 1U) %
                                        kSettingsFieldCount);
    status_ = SettingsEditorStatus::editing;
}

bool SettingsEditor::decrement() noexcept
{
    return adjust(false);
}

bool SettingsEditor::increment() noexcept
{
    return adjust(true);
}

void SettingsEditor::request_restore_defaults() noexcept
{
    if (can_edit()) {
        status_ = SettingsEditorStatus::confirm_defaults;
    }
}

void SettingsEditor::resolve_restore_defaults(const bool confirmed) noexcept
{
    if (status_ != SettingsEditorStatus::confirm_defaults) {
        return;
    }
    if (!confirmed) {
        status_ = SettingsEditorStatus::editing;
        return;
    }
    const auto selected_track = draft_.selected_track_id;
    draft_ = {};
    draft_.selected_track_id = selected_track;
    status_ = SettingsEditorStatus::defaults_staged;
}

void SettingsEditor::cancel() noexcept
{
    if (status_ == SettingsEditorStatus::locked_active) {
        status_ = SettingsEditorStatus::closed;
        return;
    }
    draft_ = original_;
    status_ = SettingsEditorStatus::cancelled;
}

settings::SettingsApplyResult SettingsEditor::save(settings::SettingsManager& manager,
                                                   const bool session_active) noexcept
{
    if (!can_edit()) {
        return settings::SettingsApplyResult::invalid_settings;
    }
    const auto result = manager.apply(draft_, session_active);
    switch (result) {
    case settings::SettingsApplyResult::applied:
        original_ = draft_;
        status_ = SettingsEditorStatus::applied;
        break;
    case settings::SettingsApplyResult::deferred:
        status_ = SettingsEditorStatus::deferred;
        break;
    case settings::SettingsApplyResult::invalid_settings:
        status_ = SettingsEditorStatus::invalid;
        break;
    case settings::SettingsApplyResult::storage_error:
        status_ = SettingsEditorStatus::storage_error;
        break;
    case settings::SettingsApplyResult::no_pending_change:
        status_ = SettingsEditorStatus::applied;
        break;
    }
    return result;
}

SettingsField SettingsEditor::field() const noexcept
{
    return field_;
}

SettingsEditorStatus SettingsEditor::status() const noexcept
{
    return status_;
}

const settings::DeviceSettings& SettingsEditor::draft() const noexcept
{
    return draft_;
}

bool SettingsEditor::changed() const noexcept
{
    return !settings::settings_equal(original_, draft_);
}

SettingsEditorViewModel SettingsEditor::view_model() const noexcept
{
    SettingsEditorViewModel model{};
    std::snprintf(model.field_name.data(), model.field_name.size(), "%s",
                  settings_field_name(field_));
    format_value(field_, draft_, model.value);
    std::snprintf(model.status.data(), model.status.size(), "%s", status_text(status_));
    model.editing = can_edit();
    model.confirming_defaults = status_ == SettingsEditorStatus::confirm_defaults;
    model.changed = changed();
    return model;
}

bool SettingsEditor::can_edit() const noexcept
{
    return status_ == SettingsEditorStatus::editing ||
           status_ == SettingsEditorStatus::defaults_staged ||
           status_ == SettingsEditorStatus::invalid ||
           status_ == SettingsEditorStatus::storage_error;
}

bool SettingsEditor::adjust(const bool forward) noexcept
{
    if (!can_edit()) {
        return false;
    }

    bool adjusted = false;
    switch (field_) {
    case SettingsField::session_duration:
        adjusted = step_choice(draft_.session_duration_seconds, kDurationSeconds, forward);
        break;
    case SettingsField::rest_duration:
        adjusted = step_choice(draft_.rest_duration_seconds, kDurationSeconds, forward);
        break;
    case SettingsField::launch_sensitivity:
        adjusted = step_choice(draft_.launch_sensitivity_milli_g, settings::kLaunchSensitivityMilliG, forward);
        break;
    case SettingsField::day_brightness:
        adjusted = step_choice(draft_.day_brightness_percent, kBrightnessPercent, forward);
        break;
    case SettingsField::night_brightness:
        adjusted = step_choice(draft_.night_brightness_percent, kBrightnessPercent, forward);
        break;
    case SettingsField::operating_mode:
        adjusted = step_enum(draft_.operating_mode, 1, forward);
        break;
    case SettingsField::trackday_mode:
        draft_.trackday_mode_enabled = !draft_.trackday_mode_enabled;
        adjusted = true;
        break;
    case SettingsField::lap_boundary:
        draft_.lap_boundary = draft_.lap_boundary == settings::LapBoundaryMode::start
                                  ? settings::LapBoundaryMode::finish
                                  : settings::LapBoundaryMode::start;
        adjusted = true;
        break;
    case SettingsField::pit_exit_auto_start:
        draft_.pit_exit_auto_start_enabled = !draft_.pit_exit_auto_start_enabled;
        adjusted = true;
        break;
    case SettingsField::pit_entry_auto_stop:
        draft_.pit_entry_auto_stop_enabled = !draft_.pit_entry_auto_stop_enabled;
        adjusted = true;
        break;
    case SettingsField::orientation:
        adjusted = step_enum(draft_.orientation, 4, forward);
        break;
    case SettingsField::auto_dim:
        draft_.auto_dim_enabled = !draft_.auto_dim_enabled;
        adjusted = true;
        break;
    case SettingsField::average_lap:
        if (forward && draft_.average_lap_seconds < 59 * 60 + 59) {
            ++draft_.average_lap_seconds;
            adjusted = true;
        }
        else if (!forward && draft_.average_lap_seconds > 0) {
            --draft_.average_lap_seconds;
            if (draft_.average_lap_seconds == 0) {
                draft_.lower_display = settings::LowerDisplayMode::elapsed;
            }
            adjusted = true;
        }
        break;
    case SettingsField::lower_display:
        if (draft_.lower_display == settings::LowerDisplayMode::elapsed &&
            draft_.average_lap_seconds == 0) {
            status_ = SettingsEditorStatus::invalid;
            return false;
        }
        draft_.lower_display = draft_.lower_display == settings::LowerDisplayMode::elapsed
                                   ? settings::LowerDisplayMode::laps_remaining
                                   : settings::LowerDisplayMode::elapsed;
        adjusted = true;
        break;
    }
    if (adjusted) {
        status_ = SettingsEditorStatus::editing;
    }
    return adjusted;
}

const char* settings_field_name(const SettingsField field) noexcept
{
    constexpr std::array<const char*, kSettingsFieldCount> names{
        "TRACK SESSION", "PIT REST",    "LAUNCH SENSITIVITY", "DAY BRIGHTNESS",
        "NIGHT BRIGHTNESS", "MODE",    "TRACKDAY MODE",      "LAP LINE",
        "PIT EXIT START",   "PIT ENTRY STOP", "ORIENTATION", "AUTO-DIM",
        "AVERAGE LAP",      "LOWER DISPLAY",
    };
    return names[static_cast<std::size_t>(field)];
}

}  // namespace track_timer::ui
