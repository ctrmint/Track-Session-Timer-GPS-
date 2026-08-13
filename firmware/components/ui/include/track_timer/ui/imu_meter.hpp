#pragma once

#include "track_timer/board/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

inline constexpr std::uint32_t kImuMeterSchemaVersion = 1;
inline constexpr std::size_t kImuTrailCapacity = 24;
inline constexpr float kStandardGravityMps2 = 9.80665F;
inline constexpr float kImuDisplayLimitG = 2.0F;
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
};

struct ImuMeterSnapshot {
    std::uint32_t schema_version{kImuMeterSchemaVersion};
    ImuMeterState state{ImuMeterState::unavailable};
    board::DisplayOrientation orientation{board::DisplayOrientation::degrees_0};
    PlanarAcceleration current{};
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
    void update_peaks(const PlanarAcceleration& point) noexcept;

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
