#include "track_timer/session/controller.hpp"

#include <algorithm>

namespace track_timer::session {

SessionController::SessionController(SessionConfiguration configuration) noexcept
{
    if (valid_configuration(configuration)) {
        snapshot_.configuration = configuration;
    }
}

TransitionResult SessionController::enter_configuration(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::ready) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    transition_to(SessionState::configuring, now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::save_configuration(
    const SessionConfiguration configuration, const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::configuring) {
        return TransitionResult::invalid_state;
    }
    if (!valid_configuration(configuration)) {
        return TransitionResult::invalid_configuration;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    snapshot_.configuration = configuration;
    enter_ready(now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::cancel_configuration(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::configuring) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    enter_ready(now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::start(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::ready) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }

    session_started_ms_ = now_ms;
    rest_started_ms_ = domain::kUnavailableTime;
    snapshot_.completion_reason = CompletionReason::none;
    snapshot_.session_elapsed_ms = 0;
    snapshot_.session_remaining_ms = snapshot_.configuration.session_duration_ms;
    snapshot_.session_overrun_ms = 0;
    snapshot_.rest_remaining_ms = domain::kUnavailableTime;
    snapshot_.stop_confirmation_pending = false;
    transition_to(SessionState::running, now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::advance(const std::int64_t now_ms) noexcept
{
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    update_for_time(now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::request_stop(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::running && snapshot_.state != SessionState::overtime &&
        snapshot_.state != SessionState::rest) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    update_for_time(now_ms);
    if (snapshot_.state != SessionState::running && snapshot_.state != SessionState::overtime &&
        snapshot_.state != SessionState::rest) {
        return TransitionResult::invalid_state;
    }
    if (!snapshot_.stop_confirmation_pending) {
        snapshot_.stop_confirmation_pending = true;
        ++snapshot_.transition_sequence;
    }
    return TransitionResult::accepted;
}

TransitionResult SessionController::cancel_stop(const std::int64_t now_ms) noexcept
{
    if (!snapshot_.stop_confirmation_pending) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    update_for_time(now_ms);
    if (!snapshot_.stop_confirmation_pending) {
        return TransitionResult::invalid_state;
    }
    snapshot_.stop_confirmation_pending = false;
    ++snapshot_.transition_sequence;
    return TransitionResult::accepted;
}

TransitionResult SessionController::confirm_stop(const std::int64_t now_ms) noexcept
{
    if (!snapshot_.stop_confirmation_pending) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    update_for_time(now_ms);

    snapshot_.stop_confirmation_pending = false;
    if (snapshot_.state == SessionState::running || snapshot_.state == SessionState::overtime) {
        refresh_session_times(now_ms);
        snapshot_.completion_reason = CompletionReason::driver_stop;
        transition_to(SessionState::review, now_ms);
        return TransitionResult::accepted;
    }
    if (snapshot_.state == SessionState::rest) {
        enter_ready(now_ms);
        return TransitionResult::accepted;
    }
    return TransitionResult::invalid_state;
}

TransitionResult SessionController::stop_from_pit_entry(
    const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::running &&
        snapshot_.state != SessionState::overtime) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }
    update_for_time(now_ms);
    if (snapshot_.state != SessionState::running &&
        snapshot_.state != SessionState::overtime) {
        return TransitionResult::invalid_state;
    }
    snapshot_.stop_confirmation_pending = false;
    refresh_session_times(now_ms);
    snapshot_.completion_reason = CompletionReason::pit_entry;
    transition_to(SessionState::review, now_ms);
    return TransitionResult::accepted;
}

TransitionResult SessionController::complete_review(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state != SessionState::review) {
        return TransitionResult::invalid_state;
    }
    if (!accept_time(now_ms)) {
        return TransitionResult::non_monotonic_time;
    }

    if (snapshot_.configuration.rest_duration_ms == 0) {
        enter_ready(now_ms);
        return TransitionResult::accepted;
    }

    rest_started_ms_ = now_ms;
    snapshot_.rest_remaining_ms = snapshot_.configuration.rest_duration_ms;
    transition_to(SessionState::rest, now_ms);
    return TransitionResult::accepted;
}

const SessionSnapshot& SessionController::snapshot() const noexcept
{
    return snapshot_;
}

bool SessionController::accept_time(const std::int64_t now_ms) noexcept
{
    if (now_ms < 0 ||
        (last_event_ms_ != domain::kUnavailableTime && now_ms < last_event_ms_)) {
        return false;
    }
    last_event_ms_ = now_ms;
    return true;
}

void SessionController::update_for_time(const std::int64_t now_ms) noexcept
{
    if (snapshot_.state == SessionState::running || snapshot_.state == SessionState::overtime) {
        refresh_session_times(now_ms);
        if (snapshot_.state == SessionState::running && snapshot_.session_remaining_ms == 0) {
            transition_to(SessionState::overtime,
                          session_started_ms_ + snapshot_.configuration.session_duration_ms);
        }
        return;
    }

    if (snapshot_.state == SessionState::rest) {
        refresh_rest_time(now_ms);
        if (snapshot_.rest_remaining_ms == 0) {
            enter_ready(rest_started_ms_ + snapshot_.configuration.rest_duration_ms);
        }
    }
}

void SessionController::transition_to(const SessionState state,
                                      const std::int64_t entered_ms) noexcept
{
    snapshot_.state = state;
    snapshot_.state_entered_ms = entered_ms;
    ++snapshot_.transition_sequence;
}

void SessionController::enter_ready(const std::int64_t entered_ms) noexcept
{
    session_started_ms_ = domain::kUnavailableTime;
    rest_started_ms_ = domain::kUnavailableTime;
    snapshot_.completion_reason = CompletionReason::none;
    snapshot_.session_elapsed_ms = domain::kUnavailableTime;
    snapshot_.session_remaining_ms = domain::kUnavailableTime;
    snapshot_.session_overrun_ms = domain::kUnavailableTime;
    snapshot_.rest_remaining_ms = domain::kUnavailableTime;
    snapshot_.stop_confirmation_pending = false;
    transition_to(SessionState::ready, entered_ms);
}

void SessionController::refresh_session_times(const std::int64_t now_ms) noexcept
{
    const auto elapsed_ms = std::max<std::int64_t>(0, now_ms - session_started_ms_);
    snapshot_.session_elapsed_ms = elapsed_ms;
    snapshot_.session_remaining_ms =
        std::max<std::int64_t>(0, snapshot_.configuration.session_duration_ms - elapsed_ms);
    snapshot_.session_overrun_ms =
        std::max<std::int64_t>(0, elapsed_ms - snapshot_.configuration.session_duration_ms);
}

void SessionController::refresh_rest_time(const std::int64_t now_ms) noexcept
{
    const auto elapsed_ms = std::max<std::int64_t>(0, now_ms - rest_started_ms_);
    snapshot_.rest_remaining_ms =
        std::max<std::int64_t>(0, snapshot_.configuration.rest_duration_ms - elapsed_ms);
}

}  // namespace track_timer::session
