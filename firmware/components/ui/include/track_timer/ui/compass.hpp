#pragma once

#include "track_timer/ui/gps_only.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind Compass mode: the receiver's course over ground.
//
// What this shows is the direction the device is *travelling*, not the direction it is
// *pointing*. The two are the same in a car and nowhere else, and they part company
// completely when it stops - because the board has a QMI8658, which is six-axis and has no
// magnetometer. There is no sensor here that can answer "which way am I facing".
//
// So a parked vehicle has no heading. Not a stale one, not north: none. That is the whole
// design problem, and it is the same shape as the two modes before this one - Clock must
// not render 00:00:00 when it does not know the time, Speed Only must not render 0 when it
// cannot see the receiver. This one is the worst of the three, because a compass that is
// merely out of date looks exactly like a compass that is right.

inline constexpr std::size_t kCompassDegreesCapacity = 4;   // "359" or "---"
inline constexpr std::size_t kCompassCardinalCapacity = 4;  // "NNE"
inline constexpr std::size_t kCompassStatusCapacity = 16;

// The receiver's own opinion of how well it knows the course, which is what gates this
// mode. headAcc widens sharply as speed falls, so it reports the thing that actually
// matters rather than requiring a speed threshold to stand in for it.
//
// Fifteen degrees is a starting point and not a researched figure. It wants measuring on
// a drive, the way the 20 Hz rate was in #16, and should be treated as a guess until then.
inline constexpr float kMaximumHeadingAccuracyDeg = 15.0F;

// A backstop under the accuracy gate, for a receiver that reports an optimistic headAcc
// while standing still. Below a walking pace there is no course to speak of whatever the
// receiver claims about it.
inline constexpr float kMinimumHeadingSpeedMps = 0.5F;

enum class CompassState : std::uint8_t {
    no_receiver,
    no_fix,
    // A fix, and still no heading. This is the state the mode exists to get right: parked
    // with sixteen satellites, the device knows exactly where it is and nothing at all
    // about which way it is facing.
    no_heading,
    live,
};

struct CompassViewModel {
    std::array<char, kCompassDegreesCapacity> degrees{};    // "090", or "---"
    std::array<char, kCompassCardinalCapacity> cardinal{};  // "E", or empty
    std::array<char, kCompassStatusCapacity> status{};
    std::uint32_t status_rgb{0};
    // Where the marker goes, in degrees clockwise from north. Only meaningful when
    // heading_valid; the screen draws nothing at all otherwise, rather than drawing this
    // at some default bearing.
    float heading_deg{0.0F};
    bool heading_valid{false};
    CompassState state{CompassState::no_receiver};
};

[[nodiscard]] CompassState compass_state(const GpsOnlySnapshot& snapshot) noexcept;
[[nodiscard]] CompassViewModel compass_view(const GpsOnlySnapshot& snapshot) noexcept;
[[nodiscard]] const char* compass_state_name(CompassState state) noexcept;

// The sixteen-point cardinal for a bearing. Exposed because the boundaries are where this
// is wrong if it is wrong: each point owns 22.5 degrees centred on itself, so north runs
// from 348.75 round through 11.25 and every other sector is offset from the round number
// a reader would guess.
[[nodiscard]] const char* compass_cardinal(float heading_deg) noexcept;

static_assert(std::is_trivially_copyable_v<CompassViewModel>);

}  // namespace track_timer::ui
