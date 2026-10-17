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
    assert(timing_config.gates.pit_entry.left.latitude_deg ==
           tracks.definitions[0].gates.pit_entry.left.latitude_deg);
    auto provisional_track = tracks.definitions[0];
    provisional_track.provenance.geometry_status =
        track::TrackGeometryStatus::provisional;
    const auto blocked_config =
        timing::make_timing_engine_config(provisional_track, settings);
    assert(blocked_config.minimum_lap_time_s == 0.0);

    session::SessionController controller{};
    auto rejected = event(timing::TimingGate::pit_exit);
    rejected.has_event = false;
    rejected.result = timing::GateCrossingResult::crossing_rejected;
    auto decision = session::apply_gate_session_automation(rejected, 0, settings,
                                                           controller);
    assert(decision.result == session::GateAutomationResult::ignored_non_event);
    assert(controller.snapshot().state == session::SessionState::ready);

    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::start), 0, settings, controller);
    assert(decision.result == session::GateAutomationResult::ignored_gate);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 0, settings, controller);
    assert(decision.result == session::GateAutomationResult::ignored_disabled);

    settings.pit_exit_auto_start_enabled = true;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 1'000, settings, controller);
    assert(decision.result == session::GateAutomationResult::started);
    assert(decision.transitioned);
    assert(decision.segment_sequence_0 == 41 && decision.segment_sequence_1 == 42);
    assert(controller.snapshot().state == session::SessionState::running);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 1'001, settings, controller);
    assert(decision.result == session::GateAutomationResult::ignored_state);

    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 2'000, settings, controller);
    assert(decision.result == session::GateAutomationResult::ignored_disabled);
    assert(controller.snapshot().state == session::SessionState::running);
    settings.pit_entry_auto_stop_enabled = true;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 2'500, settings, controller);
    assert(decision.result == session::GateAutomationResult::stopped);
    assert(controller.snapshot().state == session::SessionState::review);
    assert(controller.snapshot().completion_reason == session::CompletionReason::pit_entry);
    assert(!controller.snapshot().stop_confirmation_pending);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 2'501, settings, controller);
    assert(decision.result == session::GateAutomationResult::ignored_state);

    settings.pit_exit_auto_start_enabled = false;
    session::SessionController independently_stopped{};
    assert(independently_stopped.start(10) == session::TransitionResult::accepted);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 20, settings, independently_stopped);
    assert(decision.result == session::GateAutomationResult::stopped);

    session::SessionController overtime{{60'000, 0}};
    assert(overtime.start(0) == session::TransitionResult::accepted);
    assert(overtime.advance(60'000) == session::TransitionResult::accepted);
    assert(overtime.snapshot().state == session::SessionState::overtime);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 60'001, settings, overtime);
    assert(decision.result == session::GateAutomationResult::stopped);

    session::SessionController non_monotonic{};
    assert(non_monotonic.advance(100) == session::TransitionResult::accepted);
    settings.pit_exit_auto_start_enabled = true;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 99, settings, non_monotonic);
    assert(decision.result == session::GateAutomationResult::transition_rejected);
    assert(!decision.transitioned);

    session::SessionController manual{};
    assert(manual.start(0) == session::TransitionResult::accepted);
    assert(manual.request_stop(1'000) == session::TransitionResult::accepted);
    assert(manual.snapshot().stop_confirmation_pending);
    assert(manual.confirm_stop(1'001) == session::TransitionResult::accepted);
    assert(manual.snapshot().completion_reason == session::CompletionReason::driver_stop);

    for (const auto result : {session::GateAutomationResult::ignored_non_event,
                              session::GateAutomationResult::ignored_gate,
                              session::GateAutomationResult::ignored_disabled,
                              session::GateAutomationResult::ignored_state,
                              session::GateAutomationResult::started,
                              session::GateAutomationResult::stopped,
                              session::GateAutomationResult::transition_rejected}) {
        assert(std::strlen(session::gate_automation_result_name(result)) > 0);
    }

    // A pit exit starts the session through the controller directly, so it runs for
    // whatever configuration the controller already holds - not for whatever the driver
    // set. A controller nobody has configured starts a session at SessionConfiguration's
    // own default, which is a plausible twenty minutes and therefore the kind of wrong
    // that does not look wrong. The configuration has to be written from the settings
    // before a crossing can arrive, which is why it is no longer done only inside the
    // manual start path.
    settings.pit_exit_auto_start_enabled = true;
    session::SessionController unconfigured{};
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 0, settings, unconfigured);
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
    // save_configuration is only accepted from the configuring state, which is why
    // configure_session in the router falls back to entering it first.
    assert(configured.enter_configuration(0) == session::TransitionResult::accepted);
    assert(configured.save_configuration(configuration, 0) ==
           session::TransitionResult::accepted);
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 10, settings, configured);
    assert(decision.result == session::GateAutomationResult::started);
    assert(configured.snapshot().session_remaining_ms == configuration.session_duration_ms);
    assert(configured.snapshot().state == session::SessionState::running);

    // A second crossing while that session runs must not restart it, however many times
    // the car passes the pit exit.
    const auto started_remaining = configured.snapshot().session_remaining_ms;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_exit), 20, settings, configured);
    assert(decision.result == session::GateAutomationResult::ignored_state);
    assert(!decision.transitioned);
    assert(configured.snapshot().session_remaining_ms == started_remaining);

    // And a pit entry ends it with its own reason rather than looking like a driver stop,
    // which is what the summary records.
    settings.pit_entry_auto_stop_enabled = true;
    decision = session::apply_gate_session_automation(
        event(timing::TimingGate::pit_entry), 5'000, settings, configured);
    assert(decision.result == session::GateAutomationResult::stopped);
    assert(configured.snapshot().state == session::SessionState::review);
    assert(configured.snapshot().completion_reason == session::CompletionReason::pit_entry);

    std::cout << "Lap-gate settings and independent pit session automation passed\n";
    return 0;
}
