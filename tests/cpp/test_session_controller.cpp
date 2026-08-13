#include "track_timer/session/controller.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

using track_timer::domain::kUnavailableTime;
using track_timer::session::CompletionReason;
using track_timer::session::SessionConfiguration;
using track_timer::session::SessionController;
using track_timer::session::SessionState;
using track_timer::session::TransitionResult;
using track_timer::session::kMaximumRestDurationMs;
using track_timer::session::kMaximumSessionDurationMs;
using track_timer::session::kMinimumSessionDurationMs;

void test_configuration_boundaries()
{
    assert(track_timer::session::valid_configuration(
        SessionConfiguration{kMinimumSessionDurationMs, 0}));
    assert(track_timer::session::valid_configuration(
        SessionConfiguration{kMaximumSessionDurationMs, kMaximumRestDurationMs}));
    assert(!track_timer::session::valid_configuration(
        SessionConfiguration{kMinimumSessionDurationMs - 1, 0}));
    assert(!track_timer::session::valid_configuration(
        SessionConfiguration{kMaximumSessionDurationMs + 1, 0}));
    assert(!track_timer::session::valid_configuration(
        SessionConfiguration{kMinimumSessionDurationMs, -1}));
    assert(!track_timer::session::valid_configuration(
        SessionConfiguration{kMinimumSessionDurationMs, kMaximumRestDurationMs + 1}));
}

void test_configuration_flow()
{
    SessionController controller;
    assert(controller.snapshot().state == SessionState::ready);
    assert(controller.snapshot().configuration.session_duration_ms == 20 * 60'000);
    assert(controller.snapshot().configuration.rest_duration_ms == 20 * 60'000);
    assert(controller.start(-1) == TransitionResult::non_monotonic_time);

    assert(controller.enter_configuration(0) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::configuring);
    assert(controller.start(0) == TransitionResult::invalid_state);
    assert(controller.save_configuration(SessionConfiguration{59'999, 0}, 1) ==
           TransitionResult::invalid_configuration);
    assert(controller.snapshot().state == SessionState::configuring);
    assert(controller.cancel_configuration(1) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::ready);
    assert(controller.snapshot().configuration.session_duration_ms == 20 * 60'000);

    assert(controller.enter_configuration(2) == TransitionResult::accepted);
    assert(controller.save_configuration(SessionConfiguration{5 * 60'000, 2 * 60'000}, 3) ==
           TransitionResult::accepted);
    assert(controller.snapshot().configuration.session_duration_ms == 5 * 60'000);
    assert(controller.snapshot().configuration.rest_duration_ms == 2 * 60'000);
}

void test_running_overtime_review_and_rest()
{
    SessionController controller{SessionConfiguration{60'000, 30'000}};
    assert(controller.start(1'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::running);
    assert(controller.snapshot().session_remaining_ms == 60'000);

    assert(controller.advance(60'999) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::running);
    assert(controller.snapshot().session_elapsed_ms == 59'999);
    assert(controller.snapshot().session_remaining_ms == 1);
    assert(controller.snapshot().session_overrun_ms == 0);

    assert(controller.request_stop(60'999) == TransitionResult::accepted);
    assert(controller.snapshot().stop_confirmation_pending);
    assert(controller.advance(61'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::overtime);
    assert(controller.snapshot().state_entered_ms == 61'000);
    assert(controller.snapshot().session_remaining_ms == 0);
    assert(controller.snapshot().stop_confirmation_pending);

    assert(controller.cancel_stop(61'500) == TransitionResult::accepted);
    assert(!controller.snapshot().stop_confirmation_pending);
    assert(controller.request_stop(62'000) == TransitionResult::accepted);
    assert(controller.confirm_stop(63'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::review);
    assert(controller.snapshot().completion_reason == CompletionReason::driver_stop);
    assert(controller.snapshot().session_elapsed_ms == 62'000);
    assert(controller.snapshot().session_overrun_ms == 2'000);
    assert(!controller.snapshot().stop_confirmation_pending);

    assert(controller.advance(90'000) == TransitionResult::accepted);
    assert(controller.snapshot().session_elapsed_ms == 62'000);
    assert(controller.complete_review(90'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::rest);
    assert(controller.snapshot().rest_remaining_ms == 30'000);

    assert(controller.advance(119'999) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::rest);
    assert(controller.snapshot().rest_remaining_ms == 1);
    assert(controller.advance(120'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::ready);
    assert(controller.snapshot().session_elapsed_ms == kUnavailableTime);
    assert(controller.snapshot().rest_remaining_ms == kUnavailableTime);
}

void test_running_stop_and_disabled_rest()
{
    SessionController controller{SessionConfiguration{60'000, 0}};
    assert(controller.start(100) == TransitionResult::accepted);
    assert(controller.request_stop(1'100) == TransitionResult::accepted);
    assert(controller.confirm_stop(2'100) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::review);
    assert(controller.snapshot().session_elapsed_ms == 2'000);
    assert(controller.complete_review(3'100) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::ready);
}

void test_rest_can_be_skipped_deliberately()
{
    SessionController controller{SessionConfiguration{60'000, 30'000}};
    assert(controller.start(0) == TransitionResult::accepted);
    assert(controller.request_stop(1'000) == TransitionResult::accepted);
    assert(controller.confirm_stop(2'000) == TransitionResult::accepted);
    assert(controller.complete_review(3'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::rest);
    assert(controller.request_stop(4'000) == TransitionResult::accepted);
    assert(controller.confirm_stop(5'000) == TransitionResult::accepted);
    assert(controller.snapshot().state == SessionState::ready);
}

void test_rest_expiry_wins_over_pending_confirmation()
{
    SessionController controller{SessionConfiguration{60'000, 30'000}};
    assert(controller.start(0) == TransitionResult::accepted);
    assert(controller.request_stop(1'000) == TransitionResult::accepted);
    assert(controller.confirm_stop(2'000) == TransitionResult::accepted);
    assert(controller.complete_review(3'000) == TransitionResult::accepted);
    assert(controller.request_stop(32'999) == TransitionResult::accepted);
    assert(controller.cancel_stop(33'000) == TransitionResult::invalid_state);
    assert(controller.snapshot().state == SessionState::ready);
    assert(!controller.snapshot().stop_confirmation_pending);
}

void test_invalid_and_non_monotonic_events()
{
    SessionController controller;
    assert(controller.request_stop(0) == TransitionResult::invalid_state);
    assert(controller.confirm_stop(0) == TransitionResult::invalid_state);
    assert(controller.complete_review(0) == TransitionResult::invalid_state);
    assert(controller.start(10) == TransitionResult::accepted);
    assert(controller.advance(9) == TransitionResult::non_monotonic_time);
    assert(controller.snapshot().state == SessionState::running);
    assert(controller.enter_configuration(11) == TransitionResult::invalid_state);
    assert(controller.cancel_stop(11) == TransitionResult::invalid_state);
}

}  // namespace

int main()
{
    test_configuration_boundaries();
    test_configuration_flow();
    test_running_overtime_review_and_rest();
    test_running_stop_and_disabled_rest();
    test_rest_can_be_skipped_deliberately();
    test_rest_expiry_wins_over_pending_confirmation();
    test_invalid_and_non_monotonic_events();
    std::cout << "Session controller transitions passed\n";
    return 0;
}
