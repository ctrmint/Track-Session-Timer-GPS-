#include "track_timer/ui/display_policy.hpp"

#include <algorithm>
#include <array>

namespace track_timer::ui {
namespace {

board::DisplayOrientation fixed_orientation(
    const settings::OrientationMode orientation) noexcept
{
    switch (orientation) {
    case settings::OrientationMode::fixed_0:
        return board::DisplayOrientation::degrees_0;
    case settings::OrientationMode::fixed_90:
        return board::DisplayOrientation::degrees_90;
    case settings::OrientationMode::fixed_180:
        return board::DisplayOrientation::degrees_180;
    case settings::OrientationMode::fixed_270:
        return board::DisplayOrientation::degrees_270;
    case settings::OrientationMode::automatic:
        break;
    }
    return board::DisplayOrientation::degrees_0;
}

std::uint64_t elapsed_since(const std::uint64_t now_ms,
                            const std::uint64_t started_ms) noexcept
{
    return now_ms >= started_ms ? now_ms - started_ms : 0;
}

}  // namespace

const DisplayPolicySnapshot& DisplayPolicyController::update(
    const DisplayPolicyInput& input) noexcept
{
    const auto entering_active = input.context == DisplayContext::active && !active_;
    const auto leaving_active = input.context != DisplayContext::active && active_;
    if (leaving_active) {
        idle_started_ = false;
    }

    snapshot_.brightness_profile = input.brightness_profile;
    snapshot_.settings_preview = input.settings_preview;
    snapshot_.orientation_deferred = false;

    if (input.context == DisplayContext::active) {
        if (entering_active) {
            snapshot_.command.brightness_percent =
                input.brightness_profile == BrightnessProfile::day
                    ? input.settings.day_brightness_percent
                    : input.settings.night_brightness_percent;
            snapshot_.command.orientation = resolve_orientation(input);
        }
        else if (snapshot_.command.orientation != resolve_orientation(input)) {
            snapshot_.orientation_deferred = true;
        }
        snapshot_.command.dimmed = false;
        snapshot_.command.layout_shift_x = 0;
        snapshot_.command.layout_shift_y = 0;
        idle_started_ = false;
        active_ = true;
        return snapshot_;
    }

    active_ = false;
    snapshot_.command.brightness_percent =
        input.brightness_profile == BrightnessProfile::day
            ? input.settings.day_brightness_percent
            : input.settings.night_brightness_percent;
    snapshot_.command.orientation = resolve_orientation(input);
    snapshot_.command.dimmed = false;
    snapshot_.command.layout_shift_x = 0;
    snapshot_.command.layout_shift_y = 0;

    if (input.context == DisplayContext::ready && input.stationary) {
        if (input.user_activity || !idle_started_) {
            idle_started_ms_ = input.now_ms;
            idle_started_ = true;
        }
        apply_idle_policy(input);
    }
    else {
        idle_started_ = false;
    }
    return snapshot_;
}

const DisplayPolicySnapshot& DisplayPolicyController::snapshot() const noexcept
{
    return snapshot_;
}

board::DisplayOrientation DisplayPolicyController::resolve_orientation(
    const DisplayPolicyInput& input) const noexcept
{
    if (input.settings.orientation != settings::OrientationMode::automatic) {
        return fixed_orientation(input.settings.orientation);
    }
    if (input.sensed_orientation_available) {
        return input.sensed_orientation;
    }
    return snapshot_.command.orientation;
}

void DisplayPolicyController::apply_idle_policy(const DisplayPolicyInput& input) noexcept
{
    const auto idle_ms = elapsed_since(input.now_ms, idle_started_ms_);
    if (input.settings.auto_dim_enabled && idle_ms >= kReadyAutoDimDelayMs) {
        snapshot_.command.brightness_percent = std::min(
            snapshot_.command.brightness_percent, kDimmedBrightnessPercent);
        snapshot_.command.dimmed = true;
    }

    constexpr std::array<std::array<std::int8_t, 2>, 8> shifts{{
        {kMaximumAmoledShiftPx, 0},
        {kMaximumAmoledShiftPx, kMaximumAmoledShiftPx},
        {0, kMaximumAmoledShiftPx},
        {-kMaximumAmoledShiftPx, kMaximumAmoledShiftPx},
        {-kMaximumAmoledShiftPx, 0},
        {-kMaximumAmoledShiftPx, -kMaximumAmoledShiftPx},
        {0, -kMaximumAmoledShiftPx},
        {kMaximumAmoledShiftPx, -kMaximumAmoledShiftPx},
    }};
    if (idle_ms < kAmoledShiftIntervalMs) {
        return;
    }
    const auto index = static_cast<std::size_t>(
        (idle_ms / kAmoledShiftIntervalMs - 1U) % shifts.size());
    snapshot_.command.layout_shift_x = shifts[index][0];
    snapshot_.command.layout_shift_y = shifts[index][1];
}

const char* brightness_profile_name(const BrightnessProfile profile) noexcept
{
    return profile == BrightnessProfile::day ? "day" : "night";
}

const char* display_orientation_name(const board::DisplayOrientation orientation) noexcept
{
    switch (orientation) {
    case board::DisplayOrientation::degrees_0:
        return "0-deg";
    case board::DisplayOrientation::degrees_90:
        return "90-deg";
    case board::DisplayOrientation::degrees_180:
        return "180-deg";
    case board::DisplayOrientation::degrees_270:
        return "270-deg";
    }
    return "0-deg";
}

std::uint16_t display_orientation_degrees(
    const board::DisplayOrientation orientation) noexcept
{
    switch (orientation) {
    case board::DisplayOrientation::degrees_0:
        return 0;
    case board::DisplayOrientation::degrees_90:
        return 90;
    case board::DisplayOrientation::degrees_180:
        return 180;
    case board::DisplayOrientation::degrees_270:
        return 270;
    }
    return 0;
}

}  // namespace track_timer::ui
