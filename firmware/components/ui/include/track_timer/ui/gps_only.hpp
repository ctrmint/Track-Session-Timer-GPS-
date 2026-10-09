#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind GPS Only mode: road speed, position, and enough of the receiver's
// own report to judge whether it is working.
//
// Formatting is separated from drawing so the rule that matters here can be tested on the
// host without a panel: an absent or unfixed receiver must never render as a zero. A
// stationary car and a receiver that has never seen a satellite both have a speed of
// zero, and a diagnostic screen that cannot tell them apart is worse than no screen.

inline constexpr std::size_t kGpsOnlyRowCount = 10;

// mph, the unit on every UK speed limit sign. Metres per second is what the receiver
// reports and what the timing code works in, so it is shown alongside rather than
// converted away - this mode exists to show what the receiver said.
inline constexpr float kMetresPerSecondToMph = 2.236936F;

// Not a GnssFix on its own. Telling "no receiver attached" from "a receiver that has not
// fixed yet" is the first thing this mode is for, and a fix structure cannot express the
// former: it would have to say it with zeroes.
struct GpsOnlySnapshot {
    domain::GnssFix fix{};
    bool receiver_present{false};     // bytes have arrived from a receiver
    bool fix_valid{false};            // and that fix is current and usable
    float observed_rate_hz{0.0F};     // measured, never the configured rate
    std::uint32_t dropped_fixes{0};   // gaps in the receiver's own sequence numbering
};

enum class GpsOnlyState : std::uint8_t {
    no_receiver,
    no_fix,
    fixed,
};

struct GpsOnlyRow {
    std::array<char, 8> label{};
    std::array<char, 16> value{};
};

struct GpsOnlyViewModel {
    std::array<char, 8> speed{};       // "137" or "---", never "0" on no data
    std::array<char, 16> speed_other{};
    std::array<char, 24> status{};
    std::uint32_t status_rgb{0};
    std::array<GpsOnlyRow, kGpsOnlyRowCount> rows{};
    GpsOnlyState state{GpsOnlyState::no_receiver};
};

[[nodiscard]] GpsOnlyState gps_only_state(const GpsOnlySnapshot& snapshot) noexcept;
[[nodiscard]] GpsOnlyViewModel gps_only_view(const GpsOnlySnapshot& snapshot) noexcept;
[[nodiscard]] const char* gps_only_state_name(GpsOnlyState state) noexcept;
[[nodiscard]] const char* fix_type_label(domain::FixType type) noexcept;

static_assert(std::is_trivially_copyable_v<GpsOnlySnapshot>);
static_assert(std::is_trivially_copyable_v<GpsOnlyViewModel>);

}  // namespace track_timer::ui
