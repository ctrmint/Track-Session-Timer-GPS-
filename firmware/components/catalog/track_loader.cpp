#include "track_timer/catalog/track_loader.hpp"

#include "track_timer/timing/settings_adapter.hpp"

#include <algorithm>

namespace track_timer::catalog {

TrackApplyReport apply_track(const std::string_view track_id,
                             track::TrackDefinitionStore& store,
                             const settings::DeviceSettings& settings,
                             const bool session_active, timing::TimingEngine& engine,
                             TrackLoadScratch& scratch,
                             track::TrackDefinition& active) noexcept
{
    TrackApplyReport report{};
    const auto copied = std::min(track_id.size(), report.track_id.size() - 1);
    std::copy_n(track_id.begin(), copied, report.track_id.begin());

    if (track_id.empty()) {
        report.result = TrackApplyResult::no_selection;
        return report;
    }
    // A live session owns the timing engine. Reconfiguring gates underneath a running
    // lap would silently change what a completed lap time means.
    if (session_active) {
        report.result = TrackApplyResult::session_active;
        return report;
    }
    if (!store.exists(track_id)) {
        report.result = TrackApplyResult::definition_missing;
        return report;
    }

    scratch.blob = {};
    if (store.read(track_id, scratch.blob) != track::TrackStoreReadResult::loaded) {
        report.result = TrackApplyResult::definition_unreadable;
        return report;
    }

    // Parsed into scratch, not into `active`, so a corrupt file cannot corrupt the
    // caller's record of the currently loaded track.
    auto& candidate = scratch.candidate;
    candidate = {};
    report.load =
        track::load_track_definition({scratch.blob.bytes.data(), scratch.blob.size},
                                     candidate);
    if (report.load.result != track::TrackLoadResult::loaded) {
        report.result = TrackApplyResult::definition_invalid;
        return report;
    }
    report.revision = candidate.revision;
    report.definition_hash = candidate.definition_hash;

    // Provisional geometry stays timer-only; it must never arm a lap trigger.
    if (!track::track_timing_ready(candidate)) {
        report.result = TrackApplyResult::not_timing_ready;
        return report;
    }

    const auto config = timing::make_timing_engine_config(candidate, settings);
    report.configure = engine.configure(config);
    if (report.configure != timing::TimingEngineConfigureResult::configured) {
        report.result = TrackApplyResult::engine_rejected;
        return report;
    }

    active = candidate;
    report.engine_configured = true;
    report.result = TrackApplyResult::applied;
    return report;
}

const char* track_apply_result_name(const TrackApplyResult result) noexcept
{
    switch (result) {
    case TrackApplyResult::applied:
        return "applied";
    case TrackApplyResult::no_selection:
        return "no-selection";
    case TrackApplyResult::session_active:
        return "session-active";
    case TrackApplyResult::definition_missing:
        return "definition-missing";
    case TrackApplyResult::definition_unreadable:
        return "definition-unreadable";
    case TrackApplyResult::definition_invalid:
        return "definition-invalid";
    case TrackApplyResult::not_timing_ready:
        return "not-timing-ready";
    case TrackApplyResult::engine_rejected:
        return "engine-rejected";
    }
    return "unknown";
}

}  // namespace track_timer::catalog
