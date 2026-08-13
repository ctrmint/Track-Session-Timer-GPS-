#include "track_timer/ui/rest_session.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    using namespace track_timer;

    session::SessionController lifecycle{{60'000, 30'000}};
    assert(lifecycle.start(0) == session::TransitionResult::accepted);

    domain::UiSnapshot device{};
    device.gnss_health = domain::GnssHealth::stale;
    device.logging_available = false;
    device = ui::apply_session_timing(device, lifecycle.snapshot());
    assert(device.session_active);
    assert(device.session_remaining_ms == 60'000);
    assert(device.gnss_health == domain::GnssHealth::stale);
    assert(!device.logging_available);

    assert(lifecycle.advance(65'000) == session::TransitionResult::accepted);
    assert(lifecycle.snapshot().state == session::SessionState::overtime);
    device = ui::apply_session_timing(device, lifecycle.snapshot());
    assert(device.session_active);
    assert(device.session_remaining_ms == -5'000);
    assert(device.gnss_health == domain::GnssHealth::stale);
    assert(!device.logging_available);

    assert(lifecycle.request_stop(65'000) == session::TransitionResult::accepted);
    assert(lifecycle.confirm_stop(65'001) == session::TransitionResult::accepted);
    assert(lifecycle.snapshot().state == session::SessionState::review);
    assert(lifecycle.snapshot().completion_reason == session::CompletionReason::driver_stop);
    assert(std::strcmp(ui::completion_reason_name(lifecycle.snapshot().completion_reason),
                       "DRIVER STOP") == 0);

    assert(lifecycle.complete_review(65'002) == session::TransitionResult::accepted);
    assert(lifecycle.snapshot().state == session::SessionState::rest);
    ui::RestSessionController rest{};
    rest.update(lifecycle.snapshot(), 65'002);
    assert(rest.view_model().visible);
    assert(std::strcmp(rest.view_model().remaining.data(), "00:30") == 0);
    assert(std::strstr(rest.view_model().completion.data(), "DRIVER STOP") != nullptr);

    rest.press_skip(65'010);
    rest.release_skip(66'000);
    assert(rest.view_model().skip.state == ui::StopControlState::idle);
    assert(!rest.consume_skip_request());
    assert(lifecycle.snapshot().state == session::SessionState::rest);

    rest.press_skip(67'000);
    rest.update(lifecycle.snapshot(), 68'500);
    assert(rest.view_model().skip.state == ui::StopControlState::armed);
    rest.release_skip(68'500);
    assert(rest.view_model().skip.state == ui::StopControlState::confirming);
    rest.cancel_skip(68'600);
    assert(rest.view_model().skip.state == ui::StopControlState::idle);

    rest.press_skip(69'000);
    rest.update(lifecycle.snapshot(), 70'500);
    rest.release_skip(70'500);
    rest.confirm_skip(70'501);
    assert(rest.consume_skip_request());
    assert(lifecycle.request_stop(70'501) == session::TransitionResult::accepted);
    assert(lifecycle.confirm_stop(70'502) == session::TransitionResult::accepted);
    assert(lifecycle.snapshot().state == session::SessionState::ready);

    ui::RestSessionController expiry{};
    session::SessionController expiring{{60'000, 1'000}};
    assert(expiring.start(0) == session::TransitionResult::accepted);
    assert(expiring.request_stop(1) == session::TransitionResult::accepted);
    assert(expiring.confirm_stop(2) == session::TransitionResult::accepted);
    assert(expiring.complete_review(3) == session::TransitionResult::accepted);
    expiry.update(expiring.snapshot(), 3);
    expiry.press_skip(100);
    assert(expiring.advance(1'003) == session::TransitionResult::accepted);
    expiry.update(expiring.snapshot(), 1'003);
    assert(!expiry.view_model().visible);
    assert(!expiry.consume_skip_request());

    std::cout << "Session-driven overtime, completion, guarded rest skip, and fault independence passed\n";
    return 0;
}
