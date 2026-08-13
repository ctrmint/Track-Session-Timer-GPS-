#include "track_timer/ui/active_session.hpp"

#include "track_timer/ui/foundation.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace track_timer::ui {
namespace {

std::uint64_t deadline_after(const std::uint64_t now_ms,
                             const std::uint64_t duration_ms) noexcept
{
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return now_ms > maximum - duration_ms ? maximum : now_ms + duration_ms;
}

std::uint64_t elapsed_since(const std::uint64_t now_ms,
                            const std::uint64_t started_ms) noexcept
{
    return now_ms >= started_ms ? now_ms - started_ms : 0;
}

void format_delta(std::array<char, 32>& output, const std::int64_t delta_ms,
                  const char* suffix) noexcept
{
    const auto absolute_ms = delta_ms < 0
                                 ? static_cast<std::uint64_t>(-(delta_ms + 1)) + 1U
                                 : static_cast<std::uint64_t>(delta_ms);
    std::snprintf(output.data(), output.size(), "%c%llu.%03llu %s",
                  delta_ms < 0 ? '-' : '+',
                  static_cast<unsigned long long>(absolute_ms / 1'000),
                  static_cast<unsigned long long>(absolute_ms % 1'000), suffix);
}

}  // namespace

void ActiveSessionController::update(const domain::UiSnapshot& snapshot,
                                     const std::uint64_t now_ms) noexcept
{
    const auto starting_session = snapshot.session_active && !session_active_;
    const auto effective_now = starting_session ? now_ms : std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    view_.timing = present(snapshot);

    if (!snapshot.session_active) {
        session_active_ = false;
        baseline_valid_ = false;
        view_.feedback = {};
        stop_request_pending_ = false;
        reset_interaction();
        return;
    }

    session_active_ = true;
    update_timeouts(effective_now);
    detect_lap(snapshot, effective_now);
    prior_lap_index_ = snapshot.lap_index;
    prior_best_lap_ms_ = snapshot.best_lap_ms;
    baseline_valid_ = true;
    refresh_stop_view();
}

void ActiveSessionController::press_stop(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (!session_active_ || view_.stop.state != StopControlState::idle) {
        refresh_stop_view();
        return;
    }
    hold_started_ms_ = effective_now;
    view_.stop.state = StopControlState::holding;
    refresh_stop_view();
}

void ActiveSessionController::release_stop(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (view_.stop.state != StopControlState::holding &&
        view_.stop.state != StopControlState::armed) {
        refresh_stop_view();
        return;
    }
    if (elapsed_since(effective_now, hold_started_ms_) < kStopHoldDurationMs) {
        reset_interaction();
        return;
    }
    view_.stop.state = StopControlState::confirming;
    confirmation_expires_ms_ = deadline_after(effective_now, kStopConfirmationTimeoutMs);
    refresh_stop_view();
}

void ActiveSessionController::cancel_stop_hold(const std::uint64_t now_ms) noexcept
{
    last_update_ms_ = std::max(now_ms, last_update_ms_);
    if (view_.stop.state == StopControlState::holding ||
        view_.stop.state == StopControlState::armed) {
        reset_interaction();
    }
}

void ActiveSessionController::cancel_stop(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (view_.stop.state == StopControlState::confirming) {
        reset_interaction();
    }
}

void ActiveSessionController::confirm_stop(const std::uint64_t now_ms) noexcept
{
    const auto effective_now = std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    update_timeouts(effective_now);
    if (!session_active_ || view_.stop.state != StopControlState::confirming) {
        refresh_stop_view();
        return;
    }
    stop_request_pending_ = true;
    view_.stop.state = StopControlState::requested;
    refresh_stop_view();
}

bool ActiveSessionController::consume_stop_request() noexcept
{
    const auto pending = stop_request_pending_;
    stop_request_pending_ = false;
    return pending;
}

const ActiveSessionViewModel& ActiveSessionController::view_model() const noexcept
{
    return view_;
}

void ActiveSessionController::reset_interaction() noexcept
{
    hold_started_ms_ = 0;
    confirmation_expires_ms_ = 0;
    view_.stop.state = StopControlState::idle;
    refresh_stop_view();
}

void ActiveSessionController::update_timeouts(const std::uint64_t now_ms) noexcept
{
    if (view_.feedback.visible && now_ms >= feedback_expires_ms_) {
        view_.feedback = {};
    }
    if (view_.stop.state == StopControlState::holding &&
        elapsed_since(now_ms, hold_started_ms_) >= kStopHoldDurationMs) {
        view_.stop.state = StopControlState::armed;
    }
    if (view_.stop.state == StopControlState::confirming &&
        now_ms >= confirmation_expires_ms_) {
        reset_interaction();
    }
}

void ActiveSessionController::detect_lap(const domain::UiSnapshot& snapshot,
                                         const std::uint64_t now_ms) noexcept
{
    if (!baseline_valid_ || snapshot.lap_index <= prior_lap_index_ ||
        snapshot.previous_lap_ms == domain::kUnavailableTime) {
        return;
    }

    view_.feedback = {};
    view_.feedback.visible = true;
    const auto completed_lap = snapshot.lap_index > 0 ? snapshot.lap_index - 1 : 0;
    std::snprintf(view_.feedback.heading.data(), view_.feedback.heading.size(),
                  "LAP %02u COMPLETE", static_cast<unsigned>(completed_lap));
    view_.feedback.completed_lap = view_.timing.previous_lap;

    if (prior_best_lap_ms_ == domain::kUnavailableTime) {
        view_.feedback.kind = LapFeedbackKind::unavailable_best;
        view_.feedback.color_rgb = color::caution_bright;
        std::snprintf(view_.feedback.comparison.data(),
                      view_.feedback.comparison.size(), "BEST ESTABLISHED");
    }
    else if (snapshot.previous_lap_ms < prior_best_lap_ms_) {
        view_.feedback.kind = LapFeedbackKind::faster;
        view_.feedback.color_rgb = color::positive_bright;
        format_delta(view_.feedback.comparison,
                     snapshot.previous_lap_ms - prior_best_lap_ms_, "FASTER");
    }
    else if (snapshot.previous_lap_ms > prior_best_lap_ms_) {
        view_.feedback.kind = LapFeedbackKind::slower;
        view_.feedback.color_rgb = color::caution_bright;
        format_delta(view_.feedback.comparison,
                     snapshot.previous_lap_ms - prior_best_lap_ms_, "SLOWER");
    }
    else {
        view_.feedback.kind = LapFeedbackKind::matched_best;
        view_.feedback.color_rgb = color::positive_bright;
        std::snprintf(view_.feedback.comparison.data(),
                      view_.feedback.comparison.size(), "MATCHED BEST");
    }
    feedback_expires_ms_ = deadline_after(now_ms, kLapFeedbackDurationMs);
}

void ActiveSessionController::refresh_stop_view() noexcept
{
    view_.stop.hold_visible = view_.stop.state != StopControlState::confirming;
    view_.stop.confirmation_visible = view_.stop.state == StopControlState::confirming;
    const char* label = "HOLD TO STOP";
    switch (view_.stop.state) {
    case StopControlState::idle:
        break;
    case StopControlState::holding:
        label = "KEEP HOLDING";
        break;
    case StopControlState::armed:
        label = "RELEASE";
        break;
    case StopControlState::confirming:
        label = "CONFIRM STOP";
        break;
    case StopControlState::requested:
        label = "STOPPING...";
        break;
    }
    std::snprintf(view_.stop.hold_label.data(), view_.stop.hold_label.size(), "%s", label);
}

const char* lap_feedback_kind_name(const LapFeedbackKind kind) noexcept
{
    switch (kind) {
    case LapFeedbackKind::none:
        return "none";
    case LapFeedbackKind::faster:
        return "faster";
    case LapFeedbackKind::slower:
        return "slower";
    case LapFeedbackKind::unavailable_best:
        return "unavailable-best";
    case LapFeedbackKind::matched_best:
        return "matched-best";
    }
    return "none";
}

const char* stop_control_state_name(const StopControlState state) noexcept
{
    switch (state) {
    case StopControlState::idle:
        return "idle";
    case StopControlState::holding:
        return "holding";
    case StopControlState::armed:
        return "armed";
    case StopControlState::confirming:
        return "confirming";
    case StopControlState::requested:
        return "requested";
    }
    return "idle";
}

}  // namespace track_timer::ui
