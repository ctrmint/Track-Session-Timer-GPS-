#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/timing/engine.hpp"
#include "track_timer/track/definition.hpp"
#include "track_timer/track/storage.hpp"

#include <cstdint>
#include <string_view>

namespace track_timer::catalog {

enum class TrackApplyResult : std::uint8_t {
    applied,
    no_selection,
    session_active,
    definition_missing,
    definition_unreadable,
    definition_invalid,
    not_timing_ready,
    engine_rejected,
};

struct TrackApplyReport {
    TrackApplyResult result{TrackApplyResult::no_selection};
    track::TrackLoadReport load{};
    timing::TimingEngineConfigureResult configure{
        timing::TimingEngineConfigureResult::invalid_reference};
    std::array<char, track::kTrackIdCapacity> track_id{};
    std::uint32_t revision{0};
    std::uint64_t definition_hash{0};
    bool engine_configured{false};

    [[nodiscard]] constexpr bool ok() const noexcept
    {
        return result == TrackApplyResult::applied;
    }
};

// Caller-owned working memory. TrackDefinitionBlob is 16 KB and TrackDefinition is
// 3.6 KB; together they overflow any reasonable task stack, so they are supplied rather
// than declared locally. Allocate once, ideally in PSRAM, and reuse.
struct TrackLoadScratch {
    track::TrackDefinitionBlob blob{};
    track::TrackDefinition candidate{};
};

// Reads one track from the store, parses it, and applies it to the timing engine.
//
// Ordering matters and is deliberate: the definition is read, parsed and checked for
// timing readiness BEFORE the engine is touched. A card read failure or a corrupt file
// therefore leaves a previously loaded track running untouched. Only a geometry
// rejection from the engine itself can clear it, and TimingEngine::configure() resets
// on every failure path, so the engine is never left partially configured.
//
// `active` receives the parsed definition only on success, so the caller's record of
// the live track cannot drift from what the engine is actually using.
[[nodiscard]] TrackApplyReport apply_track(std::string_view track_id,
                                           track::TrackDefinitionStore& store,
                                           const settings::DeviceSettings& settings,
                                           bool session_active,
                                           timing::TimingEngine& engine,
                                           TrackLoadScratch& scratch,
                                           track::TrackDefinition& active) noexcept;

[[nodiscard]] const char* track_apply_result_name(TrackApplyResult result) noexcept;

}  // namespace track_timer::catalog
