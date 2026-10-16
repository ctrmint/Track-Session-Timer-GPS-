#pragma once

#include "track_timer/ui/gps_only.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind Speed Only mode: road speed in mph, and nothing else.
//
// Not a second copy of GPS Only. That mode is a diagnostic - 120 px of numeral under ten
// rows of fix type, satellites, position, accuracy and rate - and it is reached when
// something is wrong. This is a driving display: one number at a size that survives being
// read through a visor, with nothing on the panel competing with it.
//
// It does share GPS Only's snapshot and its state rule rather than restating them, so
// there is one definition on the device of what "no fix" means and one conversion factor
// out of metres per second.

// "137" or "---", with room for the terminator.
inline constexpr std::size_t kSpeedOnlyTextCapacity = 4;
inline constexpr std::size_t kSpeedOnlyStatusCapacity = 16;

struct SpeedOnlyViewModel {
    std::array<char, kSpeedOnlyTextCapacity> speed{};   // "137", "0", or "---"
    std::array<char, kSpeedOnlyStatusCapacity> status{};  // "MPH", "NO FIX", "NO RECEIVER"
    std::uint32_t status_rgb{0};
    // False in the two states that have no speed to show. The screen reads this rather
    // than comparing the string, so "show nothing" is decided once, here.
    bool speed_valid{false};
    GpsOnlyState state{GpsOnlyState::no_receiver};
};

// The rule this mode exists to get right.
//
// A stationary car and a dead receiver both have a speed of zero, and `0` is a perfectly
// believable reading - so an unknown speed is never rendered as a number. The case that is
// easy to get backwards is the other one: a car standing still with a good fix really is
// doing 0 mph, and must say so rather than show dashes.
[[nodiscard]] SpeedOnlyViewModel speed_only_view(const GpsOnlySnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<SpeedOnlyViewModel>);

}  // namespace track_timer::ui
