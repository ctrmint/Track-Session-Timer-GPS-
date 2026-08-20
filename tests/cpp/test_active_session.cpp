#include "track_timer/ui/active_session.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

track_timer::domain::UiSnapshot active_snapshot() noexcept
{
    return track_timer::domain::UiSnapshot{
        15 * 60'000,
        54'000,
        1 * 60'000 + 41'000,
        1 * 60'000 + 40'500,
        7,
        track_timer::domain::GnssHealth::good,
        true,
        true,
    };
}

}  // namespace

int main()
{
    using namespace track_timer;

    ui::ActiveSessionController controller;
    auto snapshot = active_snapshot();
    controller.update(snapshot, 0);
    assert(!controller.view_model().feedback.visible);
    assert(std::strcmp(controller.view_model().timing.session_remaining.data(), "15:00") == 0);

    snapshot.lap_index = 8;
    snapshot.previous_lap_ms = 1 * 60'000 + 39'750;
    snapshot.best_lap_ms = snapshot.previous_lap_ms;
    snapshot.current_lap_ms = 0;
    snapshot.session_remaining_ms -= 250;
    controller.update(snapshot, 250);
    auto feedback = controller.view_model().feedback;
    assert(feedback.visible && feedback.kind == ui::LapFeedbackKind::faster);
    assert(std::strcmp(feedback.heading.data(), "LAP 07 COMPLETE") == 0);
    assert(std::strcmp(feedback.completed_lap.data(), "1:39.750") == 0);
    assert(std::strcmp(feedback.comparison.data(), "-0.750 FASTER") == 0);
    assert(std::strcmp(controller.view_model().timing.session_remaining.data(), "14:59") == 0);
    controller.update(snapshot, 2'049);
    assert(controller.view_model().feedback.visible);
    controller.update(snapshot, 2'050);
    assert(!controller.view_model().feedback.visible);

    snapshot.lap_index = 9;
    snapshot.previous_lap_ms = 1 * 60'000 + 42'250;
    controller.update(snapshot, 2'100);
    feedback = controller.view_model().feedback;
    assert(feedback.visible && feedback.kind == ui::LapFeedbackKind::slower);
    assert(std::strcmp(feedback.comparison.data(), "+2.500 SLOWER") == 0);

    snapshot.lap_index = 10;
    snapshot.previous_lap_ms = 1 * 60'000 + 39'500;
    snapshot.best_lap_ms = snapshot.previous_lap_ms;
    controller.update(snapshot, 2'500);
    feedback = controller.view_model().feedback;
    assert(feedback.kind == ui::LapFeedbackKind::faster);
    assert(std::strcmp(feedback.heading.data(), "LAP 09 COMPLETE") == 0);
    controller.update(snapshot, 4'299);
    assert(controller.view_model().feedback.visible);
    controller.update(snapshot, 4'300);
    assert(!controller.view_model().feedback.visible);

    ui::ActiveSessionController first_lap;
    auto no_best = active_snapshot();
    no_best.lap_index = 1;
    no_best.previous_lap_ms = domain::kUnavailableTime;
    no_best.best_lap_ms = domain::kUnavailableTime;
    first_lap.update(no_best, 0);
    no_best.lap_index = 2;
    no_best.previous_lap_ms = 1 * 60'000 + 41'500;
    no_best.best_lap_ms = no_best.previous_lap_ms;
    first_lap.update(no_best, 250);
    assert(first_lap.view_model().feedback.kind ==
           ui::LapFeedbackKind::unavailable_best);
    assert(std::strcmp(first_lap.view_model().feedback.comparison.data(),
                       "BEST ESTABLISHED") == 0);

    ui::ActiveSessionController trackday;
    auto hidden = active_snapshot();
    trackday.update(hidden, 0, {100, true});
    const auto& trackday_view = trackday.view_model();
    assert(trackday_view.trackday.visible);
    assert(trackday_view.trackday.estimate_available);
    assert(std::strcmp(trackday_view.trackday.countdown.data(), "15:00") == 0);
    assert(std::strcmp(trackday_view.trackday.estimated_laps.data(), "9.0 LAPS") == 0);
    assert(trackday_view.timing.lap_label.front() == '\0');
    assert(trackday_view.timing.current_lap[0] == '\0');
    assert(trackday_view.timing.previous_lap[0] == '\0');
    assert(trackday_view.timing.best_lap[0] == '\0');
    assert(!trackday_view.feedback.visible);
    assert(hidden.current_lap_ms == 54'000);
    assert(hidden.previous_lap_ms == 1 * 60'000 + 41'000);
    assert(hidden.best_lap_ms == 1 * 60'000 + 40'500);

    hidden.lap_index = 8;
    hidden.previous_lap_ms = 1 * 60'000 + 39'750;
    hidden.best_lap_ms = hidden.previous_lap_ms;
    trackday.update(hidden, 250, {100, true});
    assert(!trackday.view_model().feedback.visible);
    assert(trackday.view_model().timing.previous_lap[0] == '\0');
    assert(hidden.previous_lap_ms == 1 * 60'000 + 39'750);

    trackday.update(hidden, 300, {0, true});
    assert(!trackday.view_model().trackday.estimate_available);
    assert(std::strcmp(trackday.view_model().trackday.estimated_laps.data(), "--") == 0);
    hidden.session_remaining_ms = 15 * 60'000;
    trackday.update(hidden, 350, {7, true});
    assert(std::strcmp(trackday.view_model().trackday.estimated_laps.data(), "129 LAPS") == 0);
    hidden.session_remaining_ms = -5'000;
    trackday.update(hidden, 400, {100, true});
    assert(std::strcmp(trackday.view_model().trackday.estimated_laps.data(), "0.0 LAPS") == 0);

    ui::ActiveSessionController standard_mode;
    standard_mode.update(active_snapshot(), 0, {100, false});
    assert(!standard_mode.view_model().trackday.visible);
    assert(std::strcmp(standard_mode.view_model().timing.current_lap.data(), "0:54.000") == 0);

    ui::ActiveSessionController controls;
    controls.update(active_snapshot(), 0);
    controls.press_stop(100);
    controls.release_stop(100 + ui::kStopHoldDurationMs - 1);
    assert(controls.view_model().stop.state == ui::StopControlState::idle);
    assert(!controls.consume_stop_request());

    controls.press_stop(2'000);
    controls.cancel_stop_hold(2'000 + ui::kStopHoldDurationMs + 10);
    assert(controls.view_model().stop.state == ui::StopControlState::idle);

    controls.press_stop(4'000);
    controls.update(active_snapshot(), 4'000 + ui::kStopHoldDurationMs);
    assert(controls.view_model().stop.state == ui::StopControlState::armed);
    controls.release_stop(4'000 + ui::kStopHoldDurationMs);
    assert(controls.view_model().stop.state == ui::StopControlState::confirming);
    controls.cancel_stop(5'600);
    assert(controls.view_model().stop.state == ui::StopControlState::idle);

    controls.press_stop(6'000);
    controls.release_stop(6'000 + ui::kStopHoldDurationMs);
    assert(controls.view_model().stop.state == ui::StopControlState::confirming);
    controls.update(active_snapshot(), 6'000 + ui::kStopHoldDurationMs +
                                           ui::kStopConfirmationTimeoutMs);
    assert(controls.view_model().stop.state == ui::StopControlState::idle);

    controls.press_stop(13'000);
    controls.release_stop(13'000 + ui::kStopHoldDurationMs);
    controls.confirm_stop(14'600);
    assert(controls.view_model().stop.state == ui::StopControlState::requested);
    assert(controls.consume_stop_request());
    assert(!controls.consume_stop_request());

    auto inactive = active_snapshot();
    inactive.session_active = false;
    controls.update(inactive, 15'000);
    controls.press_stop(15'100);
    controls.release_stop(20'000);
    controls.confirm_stop(20'001);
    assert(!controls.consume_stop_request());

    // A session that runs out has to say so. The countdown field turns round and counts
    // up, and the line that carried a lap estimate reports the state instead, because an
    // estimate is meaningless once the session is over.
    ui::ActiveSessionController phases;
    auto running = active_snapshot();
    running.session_remaining_ms = 4 * 60'000 + 30'000;
    ui::ActiveSessionDisplayConfig config{};
    config.average_lap_seconds = 90;
    config.trackday_mode_enabled = true;
    config.session_duration_seconds = 20 * 60;
    config.rest_duration_seconds = 10 * 60;

    phases.update(running, 0, config);
    {
        const auto& view = phases.view_model().trackday;
        assert(view.phase == ui::RunningPhase::session);
        assert(std::strcmp(view.countdown.data(), "04:30") == 0);
        assert(ui::running_phase_caption(view.phase) == nullptr);  // the estimate shows
        assert(view.estimate_available);
        assert(ui::trackday_rgb(view.phase, view.urgency) == ui::urgency_rgb(view.urgency));
    }

    config.phase = ui::RunningPhase::overrun;
    config.overrun_ms = 95'000;
    phases.update(running, 100, config);
    {
        const auto& view = phases.view_model().trackday;
        assert(view.phase == ui::RunningPhase::overrun);
        // Counting up, and without a sign: the field is five fixed cells wide.
        assert(std::strcmp(view.countdown.data(), "01:35") == 0);
        assert(std::strcmp(ui::running_phase_caption(view.phase), "OVER RUN") == 0);
        assert(!view.estimate_available);
        assert(view.remaining_ratio == 0.0F);
        assert(ui::trackday_rgb(view.phase, view.urgency) == ui::kOverrunRgb);
    }

    config.overrun_ms = 61 * 60'000 + 7'000;  // past an hour, still minutes and seconds
    phases.update(running, 200, config);
    assert(std::strcmp(phases.view_model().trackday.countdown.data(), "61:07") == 0);

    // Rest counts down exactly as the session does, against its own duration.
    config.phase = ui::RunningPhase::rest;
    config.rest_remaining_ms = 10 * 60'000;
    phases.update(running, 300, config);
    {
        const auto& view = phases.view_model().trackday;
        assert(std::strcmp(view.countdown.data(), "10:00") == 0);
        assert(std::strcmp(ui::running_phase_caption(view.phase), "REST") == 0);
        assert(view.remaining_ratio > 0.99F);
        assert(view.urgency == ui::SessionUrgency::ample);
        assert(ui::trackday_rgb(view.phase, view.urgency) == ui::urgency_rgb(view.urgency));
    }

    config.rest_remaining_ms = 30'000;
    phases.update(running, 400, config);
    {
        const auto& view = phases.view_model().trackday;
        assert(std::strcmp(view.countdown.data(), "00:30") == 0);
        assert(view.remaining_ratio < 0.1F);
        assert(view.urgency == ui::SessionUrgency::critical);
    }

    // The overrun colour must not be mistakable for any point on the ramp.
    for (const auto urgency : {ui::SessionUrgency::ample, ui::SessionUrgency::easing,
                               ui::SessionUrgency::closing, ui::SessionUrgency::urgent,
                               ui::SessionUrgency::critical}) {
        assert(ui::urgency_rgb(urgency) != ui::kOverrunRgb);
    }

    std::cout << "Deterministic lap feedback, hold-confirm stop controls, and the "
                 "overrun and rest phases passed\n";
    return 0;
}
