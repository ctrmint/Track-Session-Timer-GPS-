#include "track_timer/timing/settings_adapter.hpp"

namespace track_timer::timing {

TimingEngineConfig make_timing_engine_config(
    const track::TrackDefinition& definition,
    const settings::DeviceSettings& settings) noexcept
{
    TimingEngineConfig config{};
    if (!track::track_timing_ready(definition)) {
        return config;
    }
    config.reference = definition.reference;
    config.gates = definition.gates;
    config.lap_boundary = settings.lap_boundary == settings::LapBoundaryMode::start
                              ? LapBoundary::start
                              : LapBoundary::finish;
    config.minimum_lap_time_s = definition.minimum_lap_time_s;
    return config;
}

}  // namespace track_timer::timing
