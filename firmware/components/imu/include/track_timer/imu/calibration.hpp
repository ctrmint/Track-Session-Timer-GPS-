#pragma once

#include "track_timer/board/platform.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::imu {

inline constexpr float kStandardGravityMps2 = 9.80665F;

// How close to 1 g the magnitude must be, and how still the device must be, before a
// sample counts toward the reference. Generous enough to succeed on a dashboard with the
// engine running, tight enough to reject a device being carried.
inline constexpr float kRestMagnitudeToleranceG = 0.12F;
inline constexpr float kRestJitterToleranceG = 0.04F;
inline constexpr std::size_t kRestSamplesRequired = 24;

enum class CalibrationState : std::uint8_t {
    collecting,  // waiting for enough consecutive still samples
    ready,       // a gravity reference is held
};

// Which way the unit faces once mounted. Gravity alone cannot distinguish these: it fixes
// "up" but says nothing about which side of the device points down the road. Everything
// else - any roll, any tilt - is learned automatically.
enum class MountFacing : std::uint8_t {
    screen_to_driver,  // the usual dashboard mounting
    screen_forward,
};

struct Vector3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct PlanarG {
    float lateral_g{0.0F};
    float longitudinal_g{0.0F};
    bool valid{false};
};

// Learns the gravity vector while the device is still, then reports acceleration with
// gravity removed and resolved into vehicle axes.
//
// This does three jobs that the meter cannot do from raw axes:
//
//  * Centres the display. Raw accelerometer output at rest is dominated by gravity, so
//    without removing it the dot sits wherever the unit happens to be tilted.
//  * Corrects sensor scale. The measured magnitude at rest must be 1 g by definition, so
//    the ratio is a scale correction. The QMI8658 on this board reads about 7.7% high
//    uncalibrated.
//  * Handles arbitrary mounting. The gravity direction fixes which way is up, so any
//    roll or tilt of the unit is resolved without the driver telling it anything.
class GravityCalibration {
  public:
    explicit GravityCalibration(MountFacing facing = MountFacing::screen_to_driver) noexcept;

    // Feed every sample. Returns the state after this one.
    CalibrationState update(const board::ImuSample& sample) noexcept;

    // Gravity-removed acceleration in vehicle axes: positive lateral is to the right,
    // positive longitudinal is acceleration. Invalid until a reference is held.
    [[nodiscard]] PlanarG resolve(const board::ImuSample& sample) const noexcept;

    void restart() noexcept;
    void set_facing(MountFacing facing) noexcept;

    [[nodiscard]] CalibrationState state() const noexcept;
    [[nodiscard]] Vector3 gravity() const noexcept;
    // 1.0 when the sensor reads a true 1 g at rest; below 1 when it reads high.
    [[nodiscard]] float scale_correction() const noexcept;
    [[nodiscard]] std::size_t still_samples() const noexcept;

  private:
    void adopt_reference() noexcept;

    MountFacing facing_{MountFacing::screen_to_driver};
    CalibrationState state_{CalibrationState::collecting};

    Vector3 reference_{};        // gravity direction, unit length
    Vector3 lateral_axis_{};     // unit, points right
    Vector3 longitudinal_axis_{};// unit, points forward
    float scale_{1.0F};

    Vector3 accumulator_{};
    Vector3 previous_{};
    std::size_t still_count_{0};
    bool has_previous_{false};
};

[[nodiscard]] const char* calibration_state_name(CalibrationState state) noexcept;

}  // namespace track_timer::imu
