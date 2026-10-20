#include "track_timer/session/gate_automation.hpp"

namespace track_timer::session {

GateAutomationDecision apply_gate_session_automation(
    const timing::GateCrossingDecision& event, const std::int64_t now_ms,
    const settings::DeviceSettings& settings, const bool armed,
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
    if (event.gate == timing::TimingGate::start) {
        if (!settings.gps_start_line_enabled) {
            decision.result = GateAutomationResult::ignored_disabled;
            return decision;
        }
        // Armed means the driver asked for this. Without the check, a start-line crossing
        // would begin a session whenever the controller happened to be ready - driving out
        // of the paddock, or past the line on the way to anywhere. The pit-exit gate this
        // replaced was obscure enough to make that unlikely; the start line is not.
        if (!armed) {
            decision.result = GateAutomationResult::ignored_unarmed;
            return decision;
        }
        // Only from ready, so the first crossing starts the session and every lap after it
        // is ignored rather than restarting the clock.
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
    case GateAutomationResult::ignored_unarmed:
        return "ignored-unarmed";
    case GateAutomationResult::started:
        return "started";
    case GateAutomationResult::transition_rejected:
        return "transition-rejected";
    }
    return "ignored-non-event";
}

}  // namespace track_timer::session
