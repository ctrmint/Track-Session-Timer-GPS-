#pragma once

#include "track_timer/board/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// 2: the vertical axis. Peaks recorded before the gravity reference was tracked with the
// gyroscope latched tilt as acceleration and had left and right swapped, so they are wrong
// rather than merely old and are not migrated.
inline constexpr std::uint32_t kImuMeterSchemaVersion = 2;
inline constexpr std::size_t kImuTrailCapacity = 24;
inline constexpr float kStandardGravityMps2 = 9.80665F;
// What the radar can draw. A dot has to stay on the display, so the drawn value is held
// inside this - but a recorded peak is a different quantity with a different purpose, and
// must not inherit a limit that exists for the dial.
inline constexpr float kImuDisplayLimitG = 2.0F;

// What a recorded peak is allowed to be. Deliberately the same figure as the summary
// validator's kMaximumCrediblePeakG - beyond it is a sensor fault rather than a heroic
// lap, and a peak past what the validator accepts would discard the entire session
// record. The two are declared in different components, so screen_router static_asserts
// the agreement where they meet rather than leaving it to coincidence.
inline constexpr float kImuRecordedLimitG = 10.0F;
inline constexpr std::uint64_t kImuRecoveredNoticeMs = 1'500;

enum class ImuMeterState : std::uint8_t {
    calibrating,
    ready,
    partial,
    unavailable,
    recovered,
};

struct ImuMeterInput {
    board::ImuSample sample{};
    board::DisplayOrientation orientation{board::DisplayOrientation::degrees_0};
    std::uint64_t now_ms{0};
    bool sample_available{false};
    bool x_axis_valid{false};
    bool y_axis_valid{false};
    bool calibrating{false};
    // Appended: the vertical axis arrives on the sample's z component.
    bool z_axis_valid{false};
};

struct PlanarAcceleration {
    float lateral_g{0.0F};
    float longitudinal_g{0.0F};
    bool lateral_valid{false};
    bool longitudinal_valid{false};
};

struct ImuPeakSummary {
    float acceleration_g{0.0F};
    float braking_g{0.0F};
    float left_g{0.0F};
    float right_g{0.0F};
    float total_g{0.0F};
    PlanarAcceleration total_position{};
    // Kept apart from the horizontal pair and from each other: a kerb strike throws the car
    // up and a compression loads it down, and they are different events to a driver.
    float up_g{0.0F};
    float down_g{0.0F};
    // True when a reading was past kImuRecordedLimitG and the peak above is therefore the
    // bound rather than the measurement.
    //
    // There is still a bound, so there is still a number that could be a clamp presented
    // as a measurement - which is the whole defect. Saying so is what makes the remaining
    // limit honest instead of simply moving the lie further out.
    bool truncated{false};
};

struct ImuMeterSnapshot {
    std::uint32_t schema_version{kImuMeterSchemaVersion};
    ImuMeterState state{ImuMeterState::unavailable};
    board::DisplayOrientation orientation{board::DisplayOrientation::degrees_0};
    PlanarAcceleration current{};
    // Unaffected by the display rotation: up is up however the unit is mounted.
    float vertical_g{0.0F};
    bool vertical_valid{false};
    ImuPeakSummary peaks{};
    std::array<PlanarAcceleration, kImuTrailCapacity> trail{};
    std::size_t trail_count{0};
    std::size_t trail_next{0};
    std::uint32_t accepted_samples{0};
    std::uint32_t rejected_samples{0};
    bool reset_allowed{true};
};

class ImuMeterController {
  public:
    const ImuMeterSnapshot& update(const ImuMeterInput& input,
                                   bool session_active) noexcept;
    [[nodiscard]] bool reset(bool session_active) noexcept;
    [[nodiscard]] const ImuMeterSnapshot& snapshot() const noexcept;
    [[nodiscard]] PlanarAcceleration trail_point(std::size_t chronological_index) const noexcept;

  private:
    void clear_measurements() noexcept;
    void append(PlanarAcceleration point) noexcept;
    void update_peaks(const PlanarAcceleration& point, float vertical_g) noexcept;

    ImuMeterSnapshot snapshot_{};
    bool loss_seen_{false};
    bool session_active_{false};
    std::uint64_t recovered_since_ms_{0};
};

[[nodiscard]] const char* imu_meter_state_name(ImuMeterState state) noexcept;
[[nodiscard]] const char* imu_meter_status_text(ImuMeterState state) noexcept;

static_assert(std::is_trivially_copyable_v<ImuMeterInput>);
static_assert(std::is_trivially_copyable_v<ImuMeterSnapshot>);

}  // namespace track_timer::ui
