#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/timing/engine.hpp"
#include "track_timer/track/definition.hpp"

namespace track_timer::timing {

[[nodiscard]] TimingEngineConfig make_timing_engine_config(
    const track::TrackDefinition& definition,
    const settings::DeviceSettings& settings) noexcept;

}  // namespace track_timer::timing
