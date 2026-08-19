#include "track_timer/ui/value_picker.hpp"

#include <algorithm>
#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr std::array<std::uint16_t, 10> kLaunchMilliG{0,     500,   1'000, 1'250, 1'500,
                                                      1'750, 2'000, 2'500, 3'500, 4'000};
constexpr std::array<std::uint8_t, 4> kBrightnessPercent{25, 50, 75, 100};


void set_text(ValueChoice& choice, const char* text) noexcept
{
    std::snprintf(choice.text.data(), choice.text.size(), "%s", text);
}

void set_number(ValueChoice& choice, const char* format, const int value) noexcept
{
    std::snprintf(choice.text.data(), choice.text.size(), format, value);
}

template <typename Container, typename Value, typename Render>
ValueChoiceList from_list(const Container& values, const Value current,
                          Render render) noexcept
{
    ValueChoiceList list{};
    list.count = std::min(values.size(), list.choices.size());
    for (std::size_t index = 0; index < list.count; ++index) {
        render(list.choices[index], values[index]);
        if (values[index] == current) {
            list.selected = index;
        }
    }
    return list;
}

ValueChoiceList from_labels(const char* const* labels, const std::size_t count,
                            const std::size_t current) noexcept
{
    ValueChoiceList list{};
    list.count = std::min(count, list.choices.size());
    for (std::size_t index = 0; index < list.count; ++index) {
        set_text(list.choices[index], labels[index]);
    }
    list.selected = current < list.count ? current : 0;
    return list;
}

}  // namespace

ValueChoiceList choices_for(const SettingsField field,
                            const settings::DeviceSettings& current) noexcept
{
    static const char* const kOffOn[] = {"OFF", "ON"};
    static const char* const kLowerDisplay[] = {"ELAPSED", "LAPS LEFT"};
    static const char* const kLapBoundary[] = {"START", "FINISH"};
    static const char* const kOrientation[] = {"0", "90", "180", "270", "AUTO"};

    switch (field) {
    // The time fields have no choice list. A curated ladder could not reach their values
    // at all - average lap stopped at 3:00 against a 59:59 range - so they are edited on
    // the two-column roller instead, and an empty list is what tells the caller that.
    case SettingsField::session_duration:
    case SettingsField::rest_duration:
    case SettingsField::average_lap:
        return {};
    case SettingsField::launch_sensitivity:
        return from_list(kLaunchMilliG, current.launch_sensitivity_milli_g,
                         [](ValueChoice& c, std::uint16_t v) {
                             if (v == 0) {
                                 set_text(c, "OFF");
                             }
                             else {
                                 std::snprintf(c.text.data(), c.text.size(), "%u.%02u G",
                                               v / 1000U, (v % 1000U) / 10U);
                             }
                         });
    case SettingsField::day_brightness:
        return from_list(kBrightnessPercent, current.day_brightness_percent,
                         [](ValueChoice& c, std::uint8_t v) {
                             set_number(c, "%u%%", static_cast<int>(v));
                         });
    case SettingsField::night_brightness:
        return from_list(kBrightnessPercent, current.night_brightness_percent,
                         [](ValueChoice& c, std::uint8_t v) {
                             set_number(c, "%u%%", static_cast<int>(v));
                         });
    case SettingsField::auto_dim:
        return from_labels(kOffOn, 2, current.auto_dim_enabled ? 1U : 0U);
    case SettingsField::pit_exit_auto_start:
        return from_labels(kOffOn, 2, current.pit_exit_auto_start_enabled ? 1U : 0U);
    case SettingsField::pit_entry_auto_stop:
        return from_labels(kOffOn, 2, current.pit_entry_auto_stop_enabled ? 1U : 0U);
    case SettingsField::lower_display:
        return from_labels(kLowerDisplay, 2,
                           static_cast<std::size_t>(current.lower_display));
    case SettingsField::lap_boundary:
        return from_labels(kLapBoundary, 2,
                           static_cast<std::size_t>(current.lap_boundary));
    case SettingsField::orientation:
        return from_labels(kOrientation, 5,
                           static_cast<std::size_t>(current.orientation));
    default:
        break;
    }
    return {};
}

bool apply_choice(const SettingsField field, const std::size_t index,
                  settings::DeviceSettings& draft) noexcept
{
    switch (field) {
    // Edited on the roller, not from a list, so there is no index to apply. Returning
    // false keeps a stale value screen from writing a value the field no longer offers.
    case SettingsField::session_duration:
    case SettingsField::rest_duration:
    case SettingsField::average_lap:
        return false;
    case SettingsField::launch_sensitivity:
        if (index >= kLaunchMilliG.size()) {
            return false;
        }
        draft.launch_sensitivity_milli_g = kLaunchMilliG[index];
        return true;
    case SettingsField::day_brightness:
        if (index >= kBrightnessPercent.size()) {
            return false;
        }
        draft.day_brightness_percent = kBrightnessPercent[index];
        return true;
    case SettingsField::night_brightness:
        if (index >= kBrightnessPercent.size()) {
            return false;
        }
        draft.night_brightness_percent = kBrightnessPercent[index];
        return true;
    case SettingsField::auto_dim:
        if (index > 1) {
            return false;
        }
        draft.auto_dim_enabled = index == 1;
        return true;
    case SettingsField::pit_exit_auto_start:
        if (index > 1) {
            return false;
        }
        draft.pit_exit_auto_start_enabled = index == 1;
        return true;
    case SettingsField::pit_entry_auto_stop:
        if (index > 1) {
            return false;
        }
        draft.pit_entry_auto_stop_enabled = index == 1;
        return true;
    case SettingsField::lower_display:
        if (index > 1 ||
            (index == 1 && draft.average_lap_seconds == 0)) {
            return false;  // laps remaining needs an average lap to count against
        }
        draft.lower_display = static_cast<settings::LowerDisplayMode>(index);
        return true;
    case SettingsField::lap_boundary:
        if (index > 1) {
            return false;
        }
        draft.lap_boundary = static_cast<settings::LapBoundaryMode>(index);
        return true;
    case SettingsField::orientation:
        if (index > 4) {
            return false;
        }
        draft.orientation = static_cast<settings::OrientationMode>(index);
        return true;
    default:
        return false;
    }
}

const char* picker_field_label(const SettingsField field) noexcept
{
    switch (field) {
    case SettingsField::session_duration:
        return "SESSION";
    case SettingsField::rest_duration:
        return "REST";
    case SettingsField::average_lap:
        return "AVERAGE LAP";
    case SettingsField::launch_sensitivity:
        return "LAUNCH";
    case SettingsField::day_brightness:
        return "DAY BRIGHT";
    case SettingsField::night_brightness:
        return "NIGHT BRIGHT";
    case SettingsField::auto_dim:
        return "AUTO DIM";
    case SettingsField::lower_display:
        return "LOWER FIELD";
    case SettingsField::lap_boundary:
        return "LAP LINE";
    case SettingsField::pit_exit_auto_start:
        return "PIT EXIT START";
    case SettingsField::pit_entry_auto_stop:
        return "PIT ENTRY STOP";
    case SettingsField::orientation:
        return "ORIENTATION";
    default:
        return "";
    }
}

}  // namespace track_timer::ui
