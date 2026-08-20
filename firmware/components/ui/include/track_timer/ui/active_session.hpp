#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/ui/presenter.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

inline constexpr std::uint64_t kLapFeedbackDurationMs = 1'800;
inline constexpr std::uint64_t kStopHoldDurationMs = 1'500;
inline constexpr std::uint64_t kStopConfirmationTimeoutMs = 5'000;

enum class LapFeedbackKind : std::uint8_t {
    none,
    faster,
    slower,
    unavailable_best,
    matched_best,
};

enum class StopControlState : std::uint8_t {
    idle,
    holding,
    armed,
    confirming,
    requested,
};

struct LapFeedbackViewModel {
    std::array<char, 24> heading{};
    std::array<char, 32> completed_lap{};
    std::array<char, 32> comparison{};
    LapFeedbackKind kind{LapFeedbackKind::none};
    std::uint32_t color_rgb{0x202020};
    bool visible{false};
};

struct StopControlViewModel {
    std::array<char, 24> hold_label{};
    StopControlState state{StopControlState::idle};
    bool hold_visible{true};
    bool confirmation_visible{false};
};

// Which clock the running screen is showing. The session counts down, the overrun counts
// up past zero, and rest counts down again through the configured break.
enum class RunningPhase : std::uint8_t {
    session,
    overrun,
    rest,
};

struct ActiveSessionDisplayConfig {
    std::uint16_t average_lap_seconds{0};
    bool trackday_mode_enabled{false};
    // Appended, not inserted: this struct is aggregate-initialised at call sites, so
    // adding a field in the middle silently reassigns every positional initialiser.
    // Needed for the decaying bar and the proportional part of the colour ramp, since
    // remaining time alone cannot say what fraction of the session is left.
    std::uint32_t session_duration_seconds{0};
    // Appended for the same reason as the field above. The session snapshot carries these
    // separately from the remaining time, and the overrun is a count up rather than a
    // negative count down.
    RunningPhase phase{RunningPhase::session};
    std::int64_t overrun_ms{0};
    std::int64_t rest_remaining_ms{0};
    std::uint32_t rest_duration_seconds{0};
};

// Urgency bands for the countdown. Named rather than raw colours so the thresholds are
// testable without pinning the palette.
enum class SessionUrgency : std::uint8_t {
    ample,     // green
    easing,    // yellow
    closing,   // amber
    urgent,    // orange
    critical,  // red
    overtime,
};

struct TrackdayModeViewModel {
    std::array<char, 32> countdown{};
    RunningPhase phase{RunningPhase::session};
    std::array<char, 24> estimated_laps{};
    // 1.0 at the start of a session falling to 0.0 at its end, for the decaying bar.
    float remaining_ratio{0.0F};
    SessionUrgency urgency{SessionUrgency::ample};
    bool estimate_available{false};
    bool visible{false};
};

// Blends a proportional ramp with an absolute floor. Proportion alone would leave a
// 60-minute session green with four minutes to run; absolute alone would open a
// 20-minute session already amber. The more urgent of the two wins.
[[nodiscard]] SessionUrgency session_urgency(std::int64_t remaining_ms,
                                             std::int64_t total_ms) noexcept;
[[nodiscard]] float session_remaining_ratio(std::int64_t remaining_ms,
                                            std::int64_t total_ms) noexcept;
[[nodiscard]] std::uint32_t urgency_rgb(SessionUrgency urgency) noexcept;

// The line under the counter: the lap estimate is meaningless once a session is over, so
// the overrun and rest phases say what the timer is doing instead. Null while running, so
// the caller falls back to the estimate.
[[nodiscard]] const char* running_phase_caption(RunningPhase phase) noexcept;

// What colour the numerals take. Deep purple for the overrun, unmistakably apart from the
// green-through-red ramp that preceded it; the ramp itself for the two counting-down
// phases. Chosen on measured contrast against the black panel rather than by name: the
// literal "deep purple" #6A0DAD manages 2.27:1, where every other state here sits between
// 5.9 and 10.5, and a 10 mm numeral at 2.27:1 is hard work in daylight.
inline constexpr std::uint32_t kOverrunRgb = 0x9A4DFF;
[[nodiscard]] std::uint32_t trackday_rgb(RunningPhase phase, SessionUrgency urgency) noexcept;
[[nodiscard]] const char* session_urgency_name(SessionUrgency urgency) noexcept;

struct ActiveSessionViewModel {
    DeviceViewModel timing{};
    LapFeedbackViewModel feedback{};
    StopControlViewModel stop{};
    TrackdayModeViewModel trackday{};
};

class ActiveSessionController {
  public:
    void update(const domain::UiSnapshot& snapshot, std::uint64_t now_ms,
                const ActiveSessionDisplayConfig& display = {}) noexcept;
    void press_stop(std::uint64_t now_ms) noexcept;
    void release_stop(std::uint64_t now_ms) noexcept;
    void cancel_stop_hold(std::uint64_t now_ms) noexcept;
    void cancel_stop(std::uint64_t now_ms) noexcept;
    void confirm_stop(std::uint64_t now_ms) noexcept;

    [[nodiscard]] bool consume_stop_request() noexcept;
    [[nodiscard]] const ActiveSessionViewModel& view_model() const noexcept;

  private:
    void reset_interaction() noexcept;
    void update_timeouts(std::uint64_t now_ms) noexcept;
    void detect_lap(const domain::UiSnapshot& snapshot, std::uint64_t now_ms) noexcept;
    void refresh_stop_view() noexcept;

    ActiveSessionViewModel view_{};
    std::uint64_t last_update_ms_{0};
    std::uint64_t feedback_expires_ms_{0};
    std::uint64_t hold_started_ms_{0};
    std::uint64_t confirmation_expires_ms_{0};
    std::int64_t prior_best_lap_ms_{domain::kUnavailableTime};
    std::uint32_t prior_lap_index_{0};
    bool baseline_valid_{false};
    bool session_active_{false};
    bool stop_request_pending_{false};
};

[[nodiscard]] const char* lap_feedback_kind_name(LapFeedbackKind kind) noexcept;
[[nodiscard]] const char* stop_control_state_name(StopControlState state) noexcept;

static_assert(std::is_trivially_copyable_v<LapFeedbackViewModel>);
static_assert(std::is_trivially_copyable_v<StopControlViewModel>);
static_assert(std::is_trivially_copyable_v<ActiveSessionDisplayConfig>);
static_assert(std::is_trivially_copyable_v<TrackdayModeViewModel>);
static_assert(std::is_trivially_copyable_v<ActiveSessionViewModel>);

}  // namespace track_timer::ui
