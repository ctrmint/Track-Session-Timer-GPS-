#include "track_timer/ui/display_policy.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace {

track_timer::ui::DisplayPolicyInput ready_input() noexcept
{
    track_timer::ui::DisplayPolicyInput input{};
    input.settings.day_brightness_percent = 100;
    input.settings.night_brightness_percent = 50;
    input.settings.orientation = track_timer::settings::OrientationMode::automatic;
    input.settings.auto_dim_enabled = true;
    input.context = track_timer::ui::DisplayContext::ready;
    input.stationary = true;
    input.sensed_orientation_available = true;
    input.sensed_orientation = track_timer::board::DisplayOrientation::degrees_0;
    return input;
}

}  // namespace

int main()
{
    using namespace track_timer;

    ui::DisplayPolicyController policy;
    auto input = ready_input();
    auto output = policy.update(input);
    assert(output.command.brightness_percent == 100);
    assert(!output.command.dimmed);
    assert(output.command.layout_shift_x == 0 && output.command.layout_shift_y == 0);

    input.now_ms = ui::kAmoledShiftIntervalMs - 1;
    output = policy.update(input);
    assert(output.command.layout_shift_x == 0 && output.command.layout_shift_y == 0);
    input.now_ms = ui::kAmoledShiftIntervalMs;
    output = policy.update(input);
    assert(output.command.layout_shift_x == ui::kMaximumAmoledShiftPx);
    assert(output.command.layout_shift_y == 0);
    input.now_ms = ui::kReadyAutoDimDelayMs;
    output = policy.update(input);
    assert(output.command.brightness_percent == ui::kDimmedBrightnessPercent);
    assert(output.command.dimmed);
    assert(std::abs(output.command.layout_shift_x) <= ui::kMaximumAmoledShiftPx);
    assert(std::abs(output.command.layout_shift_y) <= ui::kMaximumAmoledShiftPx);

    input.now_ms += 1;
    input.user_activity = true;
    output = policy.update(input);
    assert(output.command.brightness_percent == 100);
    assert(!output.command.dimmed);
    assert(output.command.layout_shift_x == 0 && output.command.layout_shift_y == 0);
    input.user_activity = false;

    input.brightness_profile = ui::BrightnessProfile::night;
    output = policy.update(input);
    assert(output.command.brightness_percent == 50);
    assert(output.brightness_profile == ui::BrightnessProfile::night);

    input.settings.orientation = settings::OrientationMode::fixed_270;
    output = policy.update(input);
    assert(output.command.orientation == board::DisplayOrientation::degrees_270);
    input.settings.orientation = settings::OrientationMode::automatic;
    input.sensed_orientation = board::DisplayOrientation::degrees_90;
    output = policy.update(input);
    assert(output.command.orientation == board::DisplayOrientation::degrees_90);

    input.context = ui::DisplayContext::active;
    input.brightness_profile = ui::BrightnessProfile::day;
    input.sensed_orientation = board::DisplayOrientation::degrees_90;
    output = policy.update(input);
    const auto active_brightness = output.command.brightness_percent;
    assert(active_brightness == 100);
    assert(output.command.orientation == board::DisplayOrientation::degrees_90);
    assert(!output.command.dimmed);
    assert(output.command.layout_shift_x == 0 && output.command.layout_shift_y == 0);

    input.now_ms += 10 * ui::kReadyAutoDimDelayMs;
    input.brightness_profile = ui::BrightnessProfile::night;
    input.sensed_orientation = board::DisplayOrientation::degrees_180;
    output = policy.update(input);
    assert(output.command.brightness_percent == active_brightness);
    assert(output.command.orientation == board::DisplayOrientation::degrees_90);
    assert(output.orientation_deferred);
    assert(!output.command.dimmed);

    input.context = ui::DisplayContext::ready;
    output = policy.update(input);
    assert(output.command.brightness_percent == 50);
    assert(output.command.orientation == board::DisplayOrientation::degrees_180);
    assert(!output.orientation_deferred);

    input.context = ui::DisplayContext::other;
    input.settings_preview = true;
    input.settings.day_brightness_percent = 75;
    input.brightness_profile = ui::BrightnessProfile::day;
    input.settings.orientation = settings::OrientationMode::fixed_180;
    output = policy.update(input);
    assert(output.settings_preview);
    assert(output.command.brightness_percent == 75);
    assert(output.command.orientation == board::DisplayOrientation::degrees_180);

    input.settings_preview = false;
    input.settings.day_brightness_percent = 100;
    input.settings.orientation = settings::OrientationMode::fixed_0;
    output = policy.update(input);
    assert(!output.settings_preview);
    assert(output.command.brightness_percent == 100);
    assert(output.command.orientation == board::DisplayOrientation::degrees_0);

    std::cout << "Deterministic brightness, orientation, idle dim, wake, and AMOLED shift policy passed\n";
    return 0;
}
