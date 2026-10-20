#pragma once

#include "track_timer/ui/clock.hpp"
#include "track_timer/ui/compass.hpp"
#include "track_timer/ui/speed_only.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind Navigation mode: road speed, the receiver's clock and the
// direction of travel on one screen.
//
// Not `navigation.hpp`, which is the shell's screen-to-screen navigator and has nothing to
// do with this. The two should not be merged on the strength of sharing a word.
//
// This mode composes the three view builders that already exist rather than formatting
// anything itself. Speed Only, Clock and Compass have each already settled what their
// readout does when it is not known, and those rules are tested; a fourth formatter would
// be a fourth place for them to drift. What this file owns is the one question none of the
// three can answer alone - what to do when they are all absent for the same reason.

inline constexpr std::size_t kNavigationStatusCapacity = 24;

struct NavigationSnapshot {
    // One receiver feeds all three readouts, so there is one GNSS snapshot. The clock gets
    // its own because what it shows depends on how long ago its reading arrived, which is
    // not a property of the newest fix.
    GpsOnlySnapshot gnss{};
    ClockSnapshot clock{};
};

struct NavigationViewModel {
    SpeedOnlyViewModel speed{};
    ClockViewModel time{};
    CompassViewModel heading{};
    // All three readouts absent, for one cause that is worth stating once.
    //
    // Without this the screen prints "NO RECEIVER" in three places - beside the speed,
    // under the clock and inside the compass ring - which reads as three faults rather
    // than one. The three per-readout statuses are still the right thing to show when the
    // readouts disagree, which is the common case: a receiver that has a fix and a time
    // but no course is a parked car, and that is three different things to say.
    std::array<char, kNavigationStatusCapacity> shared_fault{};
    std::uint32_t shared_fault_rgb{0};
    bool has_shared_fault{false};
};

[[nodiscard]] NavigationViewModel navigation_view(const NavigationSnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<NavigationSnapshot>);
static_assert(std::is_trivially_copyable_v<NavigationViewModel>);

}  // namespace track_timer::ui
