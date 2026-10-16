#pragma once

#include "track_timer/ui/gps_only.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind 0-X mode: the time from a standing start to a target speed.
//
// This is an instrument rather than a readout, and the difference shows in two places.
//
// The first is that **zero is not zero**. A stationary receiver reports a few centimetres
// per second of noise, not 0.000, so the instant the car began moving cannot be read off a
// sample. It is estimated instead, by extrapolating back along the acceleration between
// the first two moving fixes to where speed would have been zero. The target end is
// interpolated the same way, between the two fixes that straddle it. At 25 Hz a fix lands
// every 40 ms, so without both interpolations every time is quantised to 40 ms - which is
// the difference between an instrument and a stopwatch.
//
// The second is that **a run that stopped being measured must not look like a run that
// finished**. If the fixes dry up the number stops climbing, which is exactly what arriving
// at the target looks like. Clock has the same problem with a frozen face and answers it
// the same way: say so rather than letting a stalled reading pass for a good one.

inline constexpr std::size_t kAccelerationTimeCapacity = 7;   // "99.99" or "--.--"
inline constexpr std::size_t kAccelerationTargetCapacity = 16;  // "0-60 MPH"
inline constexpr std::size_t kAccelerationStatusCapacity = 16;

// Below this the car is treated as stationary: ready to launch, and the point a completed
// run re-arms from. Above the receiver's own noise floor at rest, well below a walking
// pace, so a car rolling in a paddock does not read as armed.
inline constexpr float kAccelerationRestMps = 0.5F;

// The speed accuracy a run has to be measured at. Wider than this and the number would be
// a reading of the receiver's uncertainty rather than of the car.
inline constexpr float kAccelerationMaximumSpeedAccuracyMps = 0.5F;

// How long without a fresh fix before a run in progress is abandoned. Four epochs at
// 25 Hz: long enough that ordinary jitter cannot trip it, short enough that a frozen
// number is never mistaken for a finished one for more than a blink.
inline constexpr std::int64_t kAccelerationStaleUs = 160'000;

// Nothing beyond this is a measurement. A 0-60 is three to fifteen seconds; a run still
// counting at a hundred seconds did not happen.
inline constexpr float kAccelerationMaximumSeconds = 99.99F;

enum class AccelerationState : std::uint8_t {
    no_receiver,
    no_fix,
    // A fix, but the receiver is not sure enough of its speed to measure against.
    no_speed,
    // Stationary and ready. Shows a literal 0.00, which is true: the car has not moved.
    armed,
    // Moving, below the target, clock climbing.
    running,
    // Target reached. The time is frozen and green.
    complete,
    // The fixes stopped mid-run. Never green, and never confusable with complete.
    abandoned,
};

struct AccelerationSnapshot {
    GpsOnlySnapshot gnss{};
    std::uint8_t target_mph{60};
    // The MCU's monotonic clock, for judging whether a run has gone stale. Shares no epoch
    // with the fix timestamps, which is why it is only ever compared with itself.
    std::int64_t now_monotonic_us{domain::kUnavailableTime};
};

struct AccelerationViewModel {
    std::array<char, kAccelerationTimeCapacity> time{};      // "5.47", or "--.--"
    std::array<char, kAccelerationTargetCapacity> target{};  // "0-60 MPH"
    std::array<char, kAccelerationStatusCapacity> status{};
    std::uint32_t status_rgb{0};
    std::uint32_t time_rgb{0};
    bool time_valid{false};
    AccelerationState state{AccelerationState::no_receiver};
};

// Holds the run. A view model alone cannot do this: the start instant has to be remembered
// across fixes, and the finish has to be computed from the pair of fixes that straddle the
// target rather than from whichever one happens to be current.
class AccelerationRun {
  public:
    void update(const AccelerationSnapshot& snapshot) noexcept;
    // Back to armed if the car is stationary, and discards a finished or abandoned run.
    void reset() noexcept;

    [[nodiscard]] AccelerationState state() const noexcept;
    [[nodiscard]] float elapsed_seconds() const noexcept;
    [[nodiscard]] AccelerationViewModel view_model(
        const AccelerationSnapshot& snapshot) const noexcept;

  private:
    AccelerationState state_{AccelerationState::no_receiver};
    // Receiver time, not arrival time. The run is measured against the clock the fixes were
    // taken on, so a late poll cannot lengthen a run.
    std::int64_t start_ns_{domain::kUnavailableTime};
    std::int64_t finish_ns_{domain::kUnavailableTime};
    std::int64_t last_fix_ns_{domain::kUnavailableTime};
    std::int64_t last_fix_arrival_us_{domain::kUnavailableTime};
    float last_speed_mps_{0.0F};
    bool has_previous_{false};
};

// Where speed would have been zero, extrapolated back along the acceleration between two
// fixes. Exposed because it is the half of the measurement with no second opinion: the
// finish can be sanity-checked against a stopwatch, the start cannot.
[[nodiscard]] std::int64_t extrapolated_zero_ns(std::int64_t first_ns, float first_mps,
                                                std::int64_t second_ns,
                                                float second_mps) noexcept;

// When speed crossed `target_mps`, interpolated between the two fixes straddling it.
[[nodiscard]] std::int64_t interpolated_crossing_ns(std::int64_t before_ns, float before_mps,
                                                    std::int64_t after_ns, float after_mps,
                                                    float target_mps) noexcept;

[[nodiscard]] const char* acceleration_state_name(AccelerationState state) noexcept;

static_assert(std::is_trivially_copyable_v<AccelerationSnapshot>);
static_assert(std::is_trivially_copyable_v<AccelerationViewModel>);

}  // namespace track_timer::ui
