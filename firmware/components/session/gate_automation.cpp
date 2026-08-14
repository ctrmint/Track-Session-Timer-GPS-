#include "track_timer/session/gate_automation.hpp"

namespace track_timer::session {

GateAutomationDecision apply_gate_session_automation(
    const timing::GateCrossingDecision& event, const std::int64_t now_ms,
    const settings::DeviceSettings& settings,
    SessionController& controller) noexcept
{
    GateAutomationDecision decision{};
    decision.event_time_ms = now_ms;
    decision.segment_sequence_0 = event.segment_sequence_0;
    decision.segment_sequence_1 = event.segment_sequence_1;
    decision.gate = event.gate;
    if (!event.has_event || event.result != timing::GateCrossingResult::event) {
        return decision;
    }

    const auto state = controller.snapshot().state;
    if (event.gate == timing::TimingGate::pit_exit) {
        if (!settings.pit_exit_auto_start_enabled) {
            decision.result = GateAutomationResult::ignored_disabled;
            return decision;
        }
        if (state != SessionState::ready) {
            decision.result = GateAutomationResult::ignored_state;
            return decision;
        }
        decision.transition_result = controller.start(now_ms);
        decision.transitioned = decision.transition_result == TransitionResult::accepted;
        decision.result = decision.transitioned ? GateAutomationResult::started
                                                : GateAutomationResult::transition_rejected;
        return decision;
    }
    if (event.gate == timing::TimingGate::pit_entry) {
        if (!settings.pit_entry_auto_stop_enabled) {
            decision.result = GateAutomationResult::ignored_disabled;
            return decision;
        }
        if (state != SessionState::running && state != SessionState::overtime) {
            decision.result = GateAutomationResult::ignored_state;
            return decision;
        }
        decision.transition_result = controller.stop_from_pit_entry(now_ms);
        decision.transitioned = decision.transition_result == TransitionResult::accepted;
        decision.result = decision.transitioned ? GateAutomationResult::stopped
                                                : GateAutomationResult::transition_rejected;
        return decision;
    }
    decision.result = GateAutomationResult::ignored_gate;
    return decision;
}

const char* gate_automation_result_name(const GateAutomationResult result) noexcept
{
    switch (result) {
    case GateAutomationResult::ignored_non_event:
        return "ignored-non-event";
    case GateAutomationResult::ignored_gate:
        return "ignored-gate";
    case GateAutomationResult::ignored_disabled:
        return "ignored-disabled";
    case GateAutomationResult::ignored_state:
        return "ignored-state";
    case GateAutomationResult::started:
        return "started";
    case GateAutomationResult::stopped:
        return "stopped";
    case GateAutomationResult::transition_rejected:
        return "transition-rejected";
    }
    return "ignored-non-event";
}

}  // namespace track_timer::session
