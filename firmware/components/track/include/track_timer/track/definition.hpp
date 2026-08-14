#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace track_timer::track {

inline constexpr std::uint16_t kCurrentTrackSchemaVersion = 2;
inline constexpr std::size_t kMaximumTrackFileBytes = 16'384;
inline constexpr std::size_t kTrackIdCapacity = 48;
inline constexpr std::size_t kTrackNameCapacity = 64;
inline constexpr std::size_t kCountryCapacity = 4;
inline constexpr std::size_t kMaximumSectorCount = 16;
inline constexpr std::size_t kSectorIdCapacity = 32;
inline constexpr std::size_t kSectorNameCapacity = 48;
inline constexpr std::size_t kProvenanceSourceCapacity = 128;
inline constexpr std::size_t kProvenanceLicenseCapacity = 48;
inline constexpr std::size_t kProvenanceTimestampCapacity = 32;

struct GeographicPoint {
    double latitude_deg{0.0};
    double longitude_deg{0.0};
};

struct LocalPoint {
    double east_m{0.0};
    double north_m{0.0};
};

struct GeofenceDefinition {
    GeographicPoint center{};
    double radius_m{0.0};
};

struct DirectedGateDefinition {
    GeographicPoint left{};
    GeographicPoint right{};
    LocalPoint local_left{};
    LocalPoint local_right{};
    double direction_heading_deg{0.0};
    double heading_tolerance_deg{0.0};
    double minimum_crossing_speed_mps{0.0};
    double rearm_corridor_m{0.0};
};

struct CircuitGateDefinitions {
    DirectedGateDefinition start{};
    DirectedGateDefinition finish{};
    DirectedGateDefinition pit_entry{};
    DirectedGateDefinition pit_exit{};
};

struct SectorDefinition {
    std::array<char, kSectorIdCapacity> sector_id{};
    std::array<char, kSectorNameCapacity> name{};
    DirectedGateDefinition gate{};
};

struct TrackProvenance {
    std::array<char, kProvenanceSourceCapacity> source{};
    std::array<char, kProvenanceLicenseCapacity> license{};
    std::array<char, kProvenanceTimestampCapacity> verified_utc{};
};

struct TrackDefinition {
    std::uint16_t schema_version{0};
    std::array<char, kTrackIdCapacity> track_id{};
    std::array<char, kTrackNameCapacity> name{};
    std::array<char, kCountryCapacity> country{};
    std::uint32_t revision{0};
    TrackProvenance provenance{};
    GeographicPoint reference{};
    GeofenceDefinition geofence{};
    CircuitGateDefinitions gates{};
    double minimum_lap_time_s{0.0};
    std::uint64_t definition_hash{0};
    std::array<SectorDefinition, kMaximumSectorCount> sectors{};
    std::uint8_t sector_count{0};
};

enum class TrackLoadResult : std::uint8_t {
    loaded,
    empty,
    file_too_large,
    invalid_json,
    missing_required_field,
    unsupported_version,
    invalid_value,
    capacity_exceeded,
    degenerate_start_gate,
    degenerate_finish_gate,
    degenerate_pit_entry_gate,
    degenerate_pit_exit_gate,
    degenerate_sector_gate,
    duplicate_sector_id,
    geometry_outside_geofence,
};

enum class TrackDefinitionField : std::uint8_t {
    none,
    root,
    schema_version,
    revision,
    track_id,
    name,
    country,
    provenance,
    reference,
    geofence,
    gates_start,
    gates_finish,
    gates_pit_entry,
    gates_pit_exit,
    timing,
    sectors,
};

struct TrackLoadReport {
    TrackLoadResult result{TrackLoadResult::empty};
    std::size_t error_offset{0};
    TrackDefinitionField error_field{TrackDefinitionField::none};
};

[[nodiscard]] TrackLoadReport load_track_definition(std::string_view json,
                                                    TrackDefinition& output) noexcept;
[[nodiscard]] std::uint64_t hash_track_definition(std::string_view bytes) noexcept;
void format_definition_hash(std::uint64_t hash, std::array<char, 17>& output) noexcept;
[[nodiscard]] const char* track_load_result_name(TrackLoadResult result) noexcept;
[[nodiscard]] const char* track_definition_field_name(
    TrackDefinitionField field) noexcept;

static_assert(std::is_trivially_copyable_v<TrackDefinition>);
static_assert(std::is_trivially_copyable_v<TrackLoadReport>);

}  // namespace track_timer::track
