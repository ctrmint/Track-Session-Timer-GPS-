#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

enum class Readiness : std::uint8_t {
    unavailable,
    degraded,
    ready,
};

// Why lap timing will or will not run for the circuit the driver has chosen. Selection
// always succeeds, including for geometry that cannot arm, so this is the only thing that
// tells them which they have. `invalid` means the definition could not be used - a file
// problem the driver should act on; `provisional` means it was read perfectly well and its
// geometry has not been validated, which is the provenance gate working rather than a
// fault, and needs a different answer from the driver. Collapsing the two tells somebody
// to suspect their card when they should be capturing gates.
//
// `suggested`, `ambiguous` and `missing` belong to automatic track matching. The simulator
// drives them today; the device does not produce them yet.
enum class ReadyTrackState : std::uint8_t {
    selected,
    suggested,
    ambiguous,
    missing,
    invalid,
    none,
    provisional,
};

struct ReadySnapshot {
    std::array<char, 48> selected_track{};
    std::uint32_t session_duration_seconds{20 * 60};
    std::uint32_t rest_duration_seconds{20 * 60};
    domain::GnssHealth gnss_health{domain::GnssHealth::unavailable};
    Readiness storage{Readiness::unavailable};
    Readiness imu{Readiness::unavailable};
    ReadyTrackState track_state{ReadyTrackState::none};
    bool logging_available{false};
    bool session_active{false};
};

struct ReadinessItem {
    std::array<char, 24> text{};
    std::uint32_t color_rgb{0xFFFFFF};
};

struct ReadyViewModel {
    // The track line carries the circuit's own state, in the carousel's colours, because
    // `timing_mode` reports GNSS health first and so says nothing about the track until
    // there is a fix. A driver checking their selection indoors has only this.
    std::array<char, 48> selected_track{};
    std::uint32_t selected_track_color_rgb{0xFFFFFF};
    std::array<char, 24> session_duration{};
    std::array<char, 24> rest_duration{};
    std::array<char, 40> timing_mode{};
    ReadinessItem gnss{};
    ReadinessItem storage{};
    ReadinessItem imu{};
    ReadinessItem logging{};
    bool start_enabled{true};
    bool setup_enabled{true};
};

struct DeviceViewModel {
    std::array<char, 16> lap_label{};
    std::array<char, 32> current_lap{};
    std::array<char, 32> previous_lap{};
    std::array<char, 32> best_lap{};
    std::array<char, 32> session_remaining{};
    std::array<char, 24> session_status{};
    std::array<char, 16> gnss_status{};
    std::array<char, 16> logging_status{};
    domain::GnssHealth gnss_health{domain::GnssHealth::unavailable};
    std::uint32_t accent_rgb{0x202020};
    std::uint32_t accent_text_rgb{0xFFFFFF};
};

[[nodiscard]] DeviceViewModel present(const domain::UiSnapshot& snapshot) noexcept;
[[nodiscard]] ReadyViewModel present_ready(const ReadySnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<ReadySnapshot>);
static_assert(std::is_trivially_copyable_v<ReadyViewModel>);
static_assert(std::is_trivially_copyable_v<DeviceViewModel>);

}  // namespace track_timer::ui
