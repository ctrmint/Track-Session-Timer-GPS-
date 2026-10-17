#pragma once

#include "track_timer/domain/contracts.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::session {

inline constexpr std::int64_t kMinimumSessionDurationMs = 60'000;
inline constexpr std::int64_t kMaximumSessionDurationMs = 24 * 60 * 60'000;
inline constexpr std::int64_t kMaximumRestDurationMs = 24 * 60 * 60'000;

enum class SessionState : std::uint8_t {
    ready,
    configuring,
    running,
    overtime,
    review,
    rest,
};

enum class CompletionReason : std::uint8_t {
    none,
    driver_stop,
};

enum class TransitionResult : std::uint8_t {
    accepted,
    invalid_state,
    invalid_configuration,
    non_monotonic_time,
};

struct SessionConfiguration {
    std::int64_t session_duration_ms{20 * 60'000};
    std::int64_t rest_duration_ms{20 * 60'000};
};

struct SessionSnapshot {
    SessionConfiguration configuration{};
    SessionState state{SessionState::ready};
    CompletionReason completion_reason{CompletionReason::none};
    std::int64_t state_entered_ms{domain::kUnavailableTime};
    std::int64_t session_elapsed_ms{domain::kUnavailableTime};
    std::int64_t session_remaining_ms{domain::kUnavailableTime};
    std::int64_t session_overrun_ms{domain::kUnavailableTime};
    std::int64_t rest_remaining_ms{domain::kUnavailableTime};
    std::uint32_t transition_sequence{0};
    bool stop_confirmation_pending{false};
};

[[nodiscard]] constexpr bool valid_configuration(
    const SessionConfiguration& configuration) noexcept
{
    return configuration.session_duration_ms >= kMinimumSessionDurationMs &&
           configuration.session_duration_ms <= kMaximumSessionDurationMs &&
           configuration.rest_duration_ms >= 0 &&
           configuration.rest_duration_ms <= kMaximumRestDurationMs;
}

class SessionController {
  public:
    explicit SessionController(SessionConfiguration configuration = {}) noexcept;

    [[nodiscard]] TransitionResult enter_configuration(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult save_configuration(SessionConfiguration configuration,
                                                      std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult cancel_configuration(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult start(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult advance(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult request_stop(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult cancel_stop(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult confirm_stop(std::int64_t now_ms) noexcept;
    [[nodiscard]] TransitionResult complete_review(std::int64_t now_ms) noexcept;

    [[nodiscard]] const SessionSnapshot& snapshot() const noexcept;

  private:
    [[nodiscard]] bool accept_time(std::int64_t now_ms) noexcept;
    void update_for_time(std::int64_t now_ms) noexcept;
    void transition_to(SessionState state, std::int64_t entered_ms) noexcept;
    void enter_ready(std::int64_t entered_ms) noexcept;
    void refresh_session_times(std::int64_t now_ms) noexcept;
    void refresh_rest_time(std::int64_t now_ms) noexcept;

    SessionSnapshot snapshot_{};
    std::int64_t last_event_ms_{domain::kUnavailableTime};
    std::int64_t session_started_ms_{domain::kUnavailableTime};
    std::int64_t rest_started_ms_{domain::kUnavailableTime};
};

static_assert(std::is_trivially_copyable_v<SessionConfiguration>);
static_assert(std::is_trivially_copyable_v<SessionSnapshot>);

}  // namespace track_timer::session
