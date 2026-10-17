#include "track_timer/session/gate_automation.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/timing/settings_adapter.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

track_timer::timing::GateCrossingDecision event(
    const track_timer::timing::TimingGate gate)
{
    track_timer::timing::GateCrossingDecision decision{};
    decision.gate = gate;
    decision.result = track_timer::timing::GateCrossingResult::event;
    decision.has_event = true;
    decision.segment_sequence_0 = 41;
    decision.segment_sequence_1 = 42;
    return decision;
}

}  // namespace

int main()
{
    using namespace track_timer;

    const auto tracks = simulator::make_track_fixture(simulator::TrackFixtureId::suggested);
    settings::DeviceSettings settings{};
    auto timing_config = timing::make_timing_engine_config(tracks.definitions[0], settings);
    assert(timing_config.lap_boundary == timing::LapBoundary::finish);
    assert(std::strcmp(tracks.definitions[0].track_id.data(), "synthetic_test_loop") == 0);
    settings.lap_boundary = settings::LapBoundaryMode::start;
    timing_config = timing::make_timing_engine_config(tracks.definitions[0], settings);
    assert(timing_config.lap_boundary == timing::LapBoundary::start);
    assert(timing_config.gates.start.left.latitude_deg ==
           tracks.definitions[0].gates.start.left.latitude_deg);
    auto provisional_track = tracks.definitions[0];
    provisional_track.provenance.geometry_status =
        track::TrackGeometryStatus::provisional;
    const auto blocked_config =
        timing::make_timing_engine_config(provisional_track, settings);
    assert(blocked_config.minimum_lap_time_s == 0.0);

    // The automation starts a session on the start line. Four things have to be true at
    // once, and each refusal is reported distinctly because each has a different remedy.
    session::SessionController controller{};

    // Not an event at all - an evaluated crossing that did not fire.
    auto rejected = event(timing::TimingGate::start);
    rejected.has_event = false;
    rejected.result = timing::GateCrossingResult::not_evaluated;
    auto decision = session::apply_gate_session_automation(rejected, 0, settings, true,
                                                           controller);
    assert(decision.result == session::GateAutomationResult::ignored_non_event);
    assert(!decision.transitioned);

    // A gate the automation has no interest in.
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::finish), 0, settings, true, controller);
    assert(decision.result == session::GateAutomationResult::ignored_gate);

    // The right gate, but the driver did not choose the GPS trigger.
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 0, settings, true, controller);
    assert(decision.result == session::GateAutomationResult::ignored_disabled);

    // Chosen, but not armed. This is the case the pit-exit gate never had to worry about:
    // the start line is crossed on the way to anywhere, so an unarmed crossing must not
    // begin a session.
    settings.gps_start_line_enabled = true;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 500, settings, false, controller);
    assert(decision.result == session::GateAutomationResult::ignored_unarmed);
    assert(!decision.transitioned);
    assert(controller.snapshot().state == session::SessionState::ready);

    // Armed, chosen, ready: it starts.
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 1'000, settings, true, controller);
    assert(decision.result == session::GateAutomationResult::started);
    assert(decision.transitioned);
    assert(decision.event_time_ms == 1'000);
    assert(decision.gate == timing::TimingGate::start);
    assert(controller.snapshot().state == session::SessionState::running);

    // And every lap after it crosses the same line, which must not restart the clock.
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 61'000, settings, true, controller);
    assert(decision.result == session::GateAutomationResult::ignored_state);
    assert(!decision.transitioned);
    assert(controller.snapshot().state == session::SessionState::running);

    // Time that cannot have happened is refused rather than absorbed.
    session::SessionController non_monotonic{};
    assert(non_monotonic.advance(100) == session::TransitionResult::accepted);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 99, settings, true, non_monotonic);
    assert(decision.result == session::GateAutomationResult::transition_rejected);
    assert(!decision.transitioned);

    // A crossing-started session runs for whatever configuration the controller holds -
    // not for whatever the driver set. A controller nobody has configured starts at
    // SessionConfiguration's own default, which is a plausible twenty minutes and
    // therefore the kind of wrong that does not look wrong.
    session::SessionController unconfigured{};
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 0, settings, true, unconfigured);
    assert(decision.result == session::GateAutomationResult::started);
    assert(unconfigured.snapshot().session_remaining_ms ==
           session::SessionConfiguration{}.session_duration_ms);

    // Configured to something other than the default, so a session that honoured the
    // default rather than the setting is distinguishable from one that honoured neither.
    session::SessionController configured{};
    session::SessionConfiguration configuration{};
    configuration.session_duration_ms = 35 * 60 * 1'000;
    configuration.rest_duration_ms = 15 * 60 * 1'000;
    assert(configuration.session_duration_ms !=
           session::SessionConfiguration{}.session_duration_ms);
    assert(configured.enter_configuration(0) == session::TransitionResult::accepted);
    assert(configured.save_configuration(configuration, 0) ==
           session::TransitionResult::accepted);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 10, settings, true, configured);
    assert(decision.result == session::GateAutomationResult::started);
    assert(configured.snapshot().session_remaining_ms == configuration.session_duration_ms);
    assert(configured.snapshot().state == session::SessionState::running);

    // Stopping is the driver's, and stays so: there is no start-line equivalent, because
    // stopping on the start line would end the session after one lap.
    assert(configured.request_stop(5'000) == session::TransitionResult::accepted);
    assert(configured.confirm_stop(5'001) == session::TransitionResult::accepted);
    assert(configured.snapshot().completion_reason == session::CompletionReason::driver_stop);

    for (const auto result : {session::GateAutomationResult::ignored_non_event,
                              session::GateAutomationResult::ignored_gate,
                              session::GateAutomationResult::ignored_disabled,
                              session::GateAutomationResult::ignored_state,
                              session::GateAutomationResult::ignored_unarmed,
                              session::GateAutomationResult::started,
                              session::GateAutomationResult::transition_rejected}) {
        assert(std::strlen(session::gate_automation_result_name(result)) > 0);
    }

    std::cout << "Lap-gate settings and start-line session automation passed\n";
    return 0;
}
