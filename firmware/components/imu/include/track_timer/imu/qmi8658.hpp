#pragma once

#include "track_timer/board/platform.hpp"

#include <cstdint>

namespace track_timer::imu {

enum class ImuStartResult : std::uint8_t {
    ready,
    already_started,
    bus_failed,
    not_detected,     // no device acknowledged either address
    wrong_device,     // something answered, but WHO_AM_I was not a QMI8658
    configure_failed,
};

// QMI8658 6-axis IMU on the shared board I2C bus.
//
// Full scales are chosen for a car rather than a phone: +/-8 g covers kerb strikes and
// heavy braking with headroom, and 512 dps covers spins without clipping. Sampled at
// 100 Hz, which is well above what the G meter displays and cheap on a shared bus.
[[nodiscard]] ImuStartResult start() noexcept;

// Fills `sample` with the most recent conversion. Returns false when the device is not
// running or the read failed; `sample.valid` mirrors that so callers downstream cannot
// mistake a failed read for a genuine zero-g reading.
[[nodiscard]] bool read(board::ImuSample& sample) noexcept;

[[nodiscard]] bool running() noexcept;
[[nodiscard]] const char* imu_start_result_name(ImuStartResult result) noexcept;

}  // namespace track_timer::imu
