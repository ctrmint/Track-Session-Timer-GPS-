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
// A still device also has a steady turn rate, whatever its zero-rate offset happens to
// be. Testing the change rather than the value keeps this independent of that offset, so
// a unit with a large one still calibrates.
inline constexpr float kRestRateJitterDps = 3.0F;
inline constexpr std::size_t kRestSamplesRequired = 24;

// Attitude tracking, which runs while the car is moving.
//
// The accelerometer is believed only while the total force is near 1 g, because that is
// the only condition under which it can be assumed to be measuring gravity and nothing
// else. The band is tighter than the rest tolerance: this gate is what stops a corner
// from being mistaken for a change of mounting angle.
inline constexpr float kAttitudeTrustToleranceG = 0.05F;
// How quickly the accelerometer pulls the estimate back once it is trusted. Long enough
// that a gentle sustained corner cannot drag the zero point with it, short enough to
// absorb gyro drift between rest periods.
inline constexpr float kAttitudeTimeConstantS = 5.0F;
// Integration is skipped over longer gaps. A stalled service tick would otherwise rotate
// the estimate by whatever the last rate happened to be, multiplied by the whole stall.
inline constexpr float kMaximumIntegrationStepS = 0.25F;

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
    // Positive is upward. Kept separate from the horizontal pair because kerbs and
    // compressions are a different kind of event from anything the tyres do in plan.
    float vertical_g{0.0F};
    bool valid{false};
};

// Learns the gravity vector while the device is still, tracks it with the gyroscope while
// the car moves, then reports acceleration with gravity removed and resolved into
// vehicle axes.
//
// This does four jobs that the meter cannot do from raw axes:
//
//  * Centres the display. Raw accelerometer output at rest is dominated by gravity, so
//    without removing it the dot sits wherever the unit happens to be tilted.
//  * Corrects sensor scale. The measured magnitude at rest must be 1 g by definition, so
//    the ratio is a scale correction. The QMI8658 on this board reads about 7.7% high
//    uncalibrated.
//  * Handles arbitrary mounting. The gravity direction fixes which way is up, so any
//    roll or tilt of the unit is resolved without the driver telling it anything.
//  * Follows the car's attitude. An accelerometer cannot tell gravity from acceleration,
//    so a reference frozen at rest turns every later change of tilt into false G at
//    sin(angle) per axis - 0.17 g on a 10 degree banked corner. The gyroscope measures
//    that rotation directly, so the reference is rotated to match rather than left behind.
class GravityCalibration {
  public:
    explicit GravityCalibration(MountFacing facing = MountFacing::screen_to_driver) noexcept;

    // Feed every sample. Returns the state after this one.
    CalibrationState update(const board::ImuSample& sample) noexcept;

    // Gravity-removed acceleration in vehicle axes: positive lateral is to the right,
    // positive longitudinal is acceleration, positive vertical is up. Invalid until a
    // reference is held.
    [[nodiscard]] PlanarG resolve(const board::ImuSample& sample) const noexcept;

    void restart() noexcept;
    void set_facing(MountFacing facing) noexcept;

    [[nodiscard]] CalibrationState state() const noexcept;
    // Unit vector along "up" in device axes, tracked live.
    [[nodiscard]] Vector3 gravity() const noexcept;
    // 1.0 when the sensor reads a true 1 g at rest; below 1 when it reads high.
    [[nodiscard]] float scale_correction() const noexcept;
    [[nodiscard]] std::size_t still_samples() const noexcept;
    // Zero-rate offset in degrees per second, measured while still. Whatever the gyro
    // reports when the device is not moving is by definition bias, and integrating it
    // unremoved is what makes gyro attitude drift.
    [[nodiscard]] Vector3 gyro_bias() const noexcept;
    // True while the estimate is being carried by the gyroscope because the accelerometer
    // is reading something other than plain gravity.
    [[nodiscard]] bool coasting() const noexcept;

  private:
    // The locally level frame the readings are resolved into. Rebuilt from the current
    // gravity estimate on every sample, so pitch and roll are followed. Yaw is never
    // integrated: forward comes from the device's own screen normal projected into the
    // horizontal plane, so there is no heading to drift.
    struct Frame {
        Vector3 up{};
        Vector3 lateral{};
        Vector3 longitudinal{};
        bool valid{false};
    };

    void adopt_reference() noexcept;
    void track_attitude(const board::ImuSample& sample, float magnitude_g) noexcept;
    [[nodiscard]] Frame frame_for(const Vector3& reference) const noexcept;

    MountFacing facing_{MountFacing::screen_to_driver};
    CalibrationState state_{CalibrationState::collecting};

    // The direction the accelerometer reports at rest, unit length, tracked while moving.
    // That reading is specific force, so at rest it points up, not down.
    Vector3 reference_{};
    float scale_{1.0F};
    Vector3 gyro_bias_{};
    bool coasting_{false};

    Vector3 accumulator_{};
    Vector3 gyro_accumulator_{};
    Vector3 previous_{};
    Vector3 previous_rates_{};
    std::size_t still_count_{0};
    bool has_previous_{false};
    std::int64_t previous_us_{0};
    bool has_previous_us_{false};
};

[[nodiscard]] const char* calibration_state_name(CalibrationState state) noexcept;

}  // namespace track_timer::imu
