#pragma once

#include "track_timer/session/controller.hpp"
#include "track_timer/settings/settings.hpp"
#include "track_timer/timing/engine.hpp"

#include <cstdint>
#include <type_traits>

namespace track_timer::session {

enum class GateAutomationResult : std::uint8_t {
    ignored_non_event,
    ignored_gate,
    ignored_disabled,
    ignored_state,
    started,
    stopped,
    transition_rejected,
};

struct GateAutomationDecision {
    std::int64_t event_time_ms{domain::kUnavailableTime};
    std::uint32_t segment_sequence_0{0};
    std::uint32_t segment_sequence_1{0};
    timing::TimingGate gate{timing::TimingGate::start};
    GateAutomationResult result{GateAutomationResult::ignored_non_event};
    TransitionResult transition_result{TransitionResult::invalid_state};
    bool transitioned{false};
};

[[nodiscard]] GateAutomationDecision apply_gate_session_automation(
    const timing::GateCrossingDecision& event, std::int64_t now_ms,
    const settings::DeviceSettings& settings,
    SessionController& controller) noexcept;

[[nodiscard]] const char* gate_automation_result_name(
    GateAutomationResult result) noexcept;

static_assert(std::is_trivially_copyable_v<GateAutomationDecision>);

}  // namespace track_timer::session
