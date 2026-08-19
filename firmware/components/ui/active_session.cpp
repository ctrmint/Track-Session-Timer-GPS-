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

void format_estimated_laps(TrackdayModeViewModel& model,
                           const std::int64_t remaining_ms,
                           const std::uint16_t average_lap_seconds) noexcept
{
    model.estimate_available = remaining_ms != domain::kUnavailableTime &&
                               average_lap_seconds > 0;
    if (!model.estimate_available) {
        std::snprintf(model.estimated_laps.data(), model.estimated_laps.size(), "--");
        return;
    }

    const auto remaining_seconds = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, remaining_ms) / 1'000);
    if (remaining_seconds < static_cast<std::uint64_t>(average_lap_seconds) * 100U) {
        const auto tenths =
            (remaining_seconds * 10U + average_lap_seconds / 2U) /
            average_lap_seconds;
        std::snprintf(model.estimated_laps.data(), model.estimated_laps.size(),
                      "%llu.%llu LAPS",
                      static_cast<unsigned long long>(tenths / 10U),
                      static_cast<unsigned long long>(tenths % 10U));
        return;
    }

    const auto rounded =
        (remaining_seconds + average_lap_seconds / 2U) / average_lap_seconds;
    std::snprintf(model.estimated_laps.data(), model.estimated_laps.size(), "%llu LAPS",
                  static_cast<unsigned long long>(rounded));
}

}  // namespace

float session_remaining_ratio(const std::int64_t remaining_ms,
                              const std::int64_t total_ms) noexcept
{
    if (total_ms <= 0 || remaining_ms == domain::kUnavailableTime || remaining_ms <= 0) {
        return 0.0F;
    }
    const auto ratio = static_cast<float>(remaining_ms) / static_cast<float>(total_ms);
    return ratio > 1.0F ? 1.0F : ratio;
}

SessionUrgency session_urgency(const std::int64_t remaining_ms,
                               const std::int64_t total_ms) noexcept
{
    if (remaining_ms == domain::kUnavailableTime) {
        return SessionUrgency::ample;
    }
    if (remaining_ms <= 0) {
        return SessionUrgency::overtime;
    }

    // Absolute floor, deliberately covering only the genuinely final minutes. Wider
    // absolute bands defeat the proportional ramp entirely: a 20 minute session begins
    // with 20 minutes left, so a 20 minute threshold would open it already yellow.
    auto absolute = SessionUrgency::ample;
    if (remaining_ms <= 2 * 60'000) {
        absolute = SessionUrgency::critical;
    }
    else if (remaining_ms <= 5 * 60'000) {
        absolute = SessionUrgency::urgent;
    }

    // Proportional ramp, so a short session still walks through every band rather than
    // opening amber and staying there.
    const auto ratio = session_remaining_ratio(remaining_ms, total_ms);
    auto proportional = SessionUrgency::ample;
    if (total_ms > 0) {
        if (ratio <= 0.10F) {
            proportional = SessionUrgency::critical;
        }
        else if (ratio <= 0.25F) {
            proportional = SessionUrgency::urgent;
        }
        else if (ratio <= 0.45F) {
            proportional = SessionUrgency::closing;
        }
        else if (ratio <= 0.70F) {
            proportional = SessionUrgency::easing;
        }
    }

    return static_cast<std::uint8_t>(absolute) >= static_cast<std::uint8_t>(proportional)
               ? absolute
               : proportional;
}

std::uint32_t urgency_rgb(const SessionUrgency urgency) noexcept
{
    switch (urgency) {
    case SessionUrgency::ample:
        return 0x2FD16D;
    case SessionUrgency::easing:
        return 0xF2E44B;
    case SessionUrgency::closing:
        return 0xFFC02E;
    case SessionUrgency::urgent:
        return 0xFF8A24;
    case SessionUrgency::critical:
    case SessionUrgency::overtime:
        return 0xFF3B30;
    }
    return 0xFFFFFF;
}

const char* session_urgency_name(const SessionUrgency urgency) noexcept
{
    switch (urgency) {
    case SessionUrgency::ample:
        return "ample";
    case SessionUrgency::easing:
        return "easing";
    case SessionUrgency::closing:
        return "closing";
    case SessionUrgency::urgent:
        return "urgent";
    case SessionUrgency::critical:
        return "critical";
    case SessionUrgency::overtime:
        return "overtime";
    }
    return "unknown";
}

void ActiveSessionController::update(const domain::UiSnapshot& snapshot,
                                     const std::uint64_t now_ms,
                                     const ActiveSessionDisplayConfig& display) noexcept
{
    const auto starting_session = snapshot.session_active && !session_active_;
    const auto effective_now = starting_session ? now_ms : std::max(now_ms, last_update_ms_);
    last_update_ms_ = effective_now;
    view_.timing = present(snapshot);
    view_.trackday = {};

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
    if (display.trackday_mode_enabled) {
        view_.feedback = {};
        view_.trackday.visible = true;
        view_.trackday.countdown = view_.timing.session_remaining;
        format_estimated_laps(view_.trackday, snapshot.session_remaining_ms,
                              display.average_lap_seconds);
        const auto total_ms =
            static_cast<std::int64_t>(display.session_duration_minutes) * 60'000;
        view_.trackday.remaining_ratio =
            session_remaining_ratio(snapshot.session_remaining_ms, total_ms);
        view_.trackday.urgency =
            session_urgency(snapshot.session_remaining_ms, total_ms);
        view_.timing.lap_label.fill('\0');
        view_.timing.current_lap.fill('\0');
        view_.timing.previous_lap.fill('\0');
        view_.timing.best_lap.fill('\0');
    }
    else {
        detect_lap(snapshot, effective_now);
    }
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
