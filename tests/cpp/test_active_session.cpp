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

    std::cout << "Deterministic lap feedback and hold-confirm stop controls passed\n";
    return 0;
}
