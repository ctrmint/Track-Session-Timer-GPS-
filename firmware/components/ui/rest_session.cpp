#include "track_timer/ui/rest_session.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace track_timer::ui {
namespace {

std::uint64_t elapsed_since(const std::uint64_t now_ms,
                            const std::uint64_t started_ms) noexcept
{
    return now_ms >= started_ms ? now_ms - started_ms : 0;
}

std::uint64_t deadline_after(const std::uint64_t now_ms,
                             const std::uint64_t duration_ms) noexcept
{
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return now_ms > maximum - duration_ms ? maximum : now_ms + duration_ms;
}

void format_duration(std::array<char, 24>& output, const std::int64_t duration_ms) noexcept
{
    if (duration_ms == domain::kUnavailableTime) {
        std::snprintf(output.data(), output.size(), "--:--");
        return;
    }
    const auto bounded = std::max<std::int64_t>(0, duration_ms);
    const auto total_seconds = static_cast<std::uint64_t>(bounded / 1'000);
    std::snprintf(output.data(), output.size(), "%02llu:%02llu",
                  static_cast<unsigned long long>(total_seconds / 60),
                  static_cast<unsigned long long>(total_seconds % 60));
}

}  // namespace

void RestSessionController::update(const session::SessionSnapshot& snapshot,
                                   const std::uint64_t now_ms) noexcept
{
    last_update_ms_ = std::max(last_update_ms_, now_ms);
    view_.visible = snapshot.state == session::SessionState::rest;
    if (!view_.visible) {
        skip_request_pending_ = false;
        reset_interaction();
        format_duration(view_.remaining, domain::kUnavailableTime);
        return;
    }

    format_duration(view_.remaining, snapshot.rest_remaining_ms);
    std::snprintf(view_.completion.data(), view_.completion.size(), "COMPLETED: %s",
                  completion_reason_name(snapshot.completion_reason));
    std::snprintf(view_.next_step.data(), view_.next_step.size(),
                  "READY AUTOMATICALLY AT 00:00");
    update_timeouts(last_update_ms_);
    refresh_skip_view();
}

void RestSessionController::press_skip(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(last_update_ms_, now_ms);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (!view_.visible || view_.skip.state != StopControlState::idle) {
        refresh_skip_view();
        return;
    }
    hold_started_ms_ = effective_now;
    view_.skip.state = StopControlState::holding;
    refresh_skip_view();
}

void RestSessionController::release_skip(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(last_update_ms_, now_ms);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (view_.skip.state != StopControlState::holding &&
        view_.skip.state != StopControlState::armed) {
        refresh_skip_view();
        return;
    }
    if (elapsed_since(effective_now, hold_started_ms_) < kStopHoldDurationMs) {
        reset_interaction();
        return;
    }
    view_.skip.state = StopControlState::confirming;
    confirmation_expires_ms_ = deadline_after(effective_now, kStopConfirmationTimeoutMs);
    refresh_skip_view();
}

void RestSessionController::cancel_skip_hold(const std::uint64_t now_ms) noexcept
{
    last_update_ms_ = std::max(last_update_ms_, now_ms);
    if (view_.skip.state == StopControlState::holding ||
        view_.skip.state == StopControlState::armed) {
        reset_interaction();
    }
}

void RestSessionController::cancel_skip(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(last_update_ms_, now_ms);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (view_.skip.state == StopControlState::confirming) {
        reset_interaction();
    }
}

void RestSessionController::confirm_skip(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(last_update_ms_, now_ms);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (!view_.visible || view_.skip.state != StopControlState::confirming) {
        refresh_skip_view();
        return;
    }
    skip_request_pending_ = true;
    view_.skip.state = StopControlState::requested;
    refresh_skip_view();
}

bool RestSessionController::consume_skip_request() noexcept
{
    const auto pending = skip_request_pending_;
    skip_request_pending_ = false;
    return pending;
}

const RestSessionViewModel& RestSessionController::view_model() const noexcept
{
    return view_;
}

void RestSessionController::reset_interaction() noexcept
{
    hold_started_ms_ = 0;
    confirmation_expires_ms_ = 0;
    view_.skip.state = StopControlState::idle;
    refresh_skip_view();
}

void RestSessionController::update_timeouts(const std::uint64_t now_ms) noexcept
{
    if (view_.skip.state == StopControlState::holding &&
        elapsed_since(now_ms, hold_started_ms_) >= kStopHoldDurationMs) {
        view_.skip.state = StopControlState::armed;
    }
    if (view_.skip.state == StopControlState::confirming &&
        now_ms >= confirmation_expires_ms_) {
        reset_interaction();
    }
}

void RestSessionController::refresh_skip_view() noexcept
{
    view_.skip.hold_visible = view_.skip.state != StopControlState::confirming;
    view_.skip.confirmation_visible = view_.skip.state == StopControlState::confirming;
    const char* label = "HOLD TO SKIP REST";
    switch (view_.skip.state) {
    case StopControlState::idle:
        break;
    case StopControlState::holding:
        label = "KEEP HOLDING";
        break;
    case StopControlState::armed:
        label = "RELEASE";
        break;
    case StopControlState::confirming:
        label = "CONFIRM SKIP";
        break;
    case StopControlState::requested:
        label = "RETURNING...";
        break;
    }
    std::snprintf(view_.skip.hold_label.data(), view_.skip.hold_label.size(), "%s", label);
}

const char* session_state_name(const session::SessionState state) noexcept
{
    switch (state) {
    case session::SessionState::ready:
        return "ready";
    case session::SessionState::configuring:
        return "configuring";
    case session::SessionState::running:
        return "running";
    case session::SessionState::overtime:
        return "overtime";
    case session::SessionState::review:
        return "review";
    case session::SessionState::rest:
        return "rest";
    }
    return "ready";
}

const char* completion_reason_name(const session::CompletionReason reason) noexcept
{
    switch (reason) {
    case session::CompletionReason::none:
        return "NOT COMPLETE";
    case session::CompletionReason::driver_stop:
        return "DRIVER STOP";
    case session::CompletionReason::pit_entry:
        return "PIT ENTRY";
    }
    return "NOT COMPLETE";
}

domain::UiSnapshot apply_session_timing(
    const domain::UiSnapshot& input,
    const session::SessionSnapshot& session_snapshot) noexcept
{
    auto output = input;
    switch (session_snapshot.state) {
    case session::SessionState::running:
        output.session_active = true;
        output.session_remaining_ms = session_snapshot.session_remaining_ms;
        break;
    case session::SessionState::overtime:
        output.session_active = true;
        output.session_remaining_ms = -session_snapshot.session_overrun_ms;
        break;
    case session::SessionState::ready:
    case session::SessionState::configuring:
    case session::SessionState::review:
    case session::SessionState::rest:
        output.session_active = false;
        break;
    }
    return output;
}

}  // namespace track_timer::ui
