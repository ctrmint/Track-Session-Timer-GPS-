#include "track_timer/track/definition.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace track_timer::track {
namespace {

constexpr double kEarthRadiusM = 6'371'000.0;
constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
constexpr std::size_t kMaximumJsonDepth = 12;

enum class StringResult : std::uint8_t {
    ok,
    overflow,
    invalid,
};

class JsonReader {
  public:
    explicit JsonReader(const std::string_view input) noexcept : input_(input) {}

    void whitespace() noexcept
    {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
                break;
            }
            ++position_;
        }
    }

    [[nodiscard]] bool consume(const char expected) noexcept
    {
        whitespace();
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    [[nodiscard]] char peek() noexcept
    {
        whitespace();
        return position_ < input_.size() ? input_[position_] : '\0';
    }

    [[nodiscard]] bool finished() noexcept
    {
        whitespace();
        return position_ == input_.size();
    }

    [[nodiscard]] std::size_t position() const noexcept
    {
        return position_;
    }

    template <std::size_t Capacity>
    [[nodiscard]] StringResult string(std::array<char, Capacity>& output) noexcept
    {
        output.fill('\0');
        return string_impl(output.data(), Capacity);
    }

    [[nodiscard]] bool number(double& output) noexcept
    {
        whitespace();
        const auto start = position_;
        bool negative = false;
        if (take('-')) {
            negative = true;
        }
        if (position_ >= input_.size()) {
            position_ = start;
            return false;
        }

        double value = 0.0;
        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size() && input_[position_] >= '0' &&
                input_[position_] <= '9') {
                position_ = start;
                return false;
            }
        }
        else if (input_[position_] >= '1' && input_[position_] <= '9') {
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                value = value * 10.0 + static_cast<double>(input_[position_] - '0');
                ++position_;
            }
        }
        else {
            position_ = start;
            return false;
        }

        if (take('.')) {
            if (position_ >= input_.size() || input_[position_] < '0' ||
                input_[position_] > '9') {
                position_ = start;
                return false;
            }
            double scale = 0.1;
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                value += static_cast<double>(input_[position_] - '0') * scale;
                scale *= 0.1;
                ++position_;
            }
        }

        int exponent = 0;
        bool exponent_negative = false;
        if (take('e') || take('E')) {
            if (take('-')) {
                exponent_negative = true;
            }
            else {
                (void)take('+');
            }
            if (position_ >= input_.size() || input_[position_] < '0' ||
                input_[position_] > '9') {
                position_ = start;
                return false;
            }
            while (position_ < input_.size() && input_[position_] >= '0' &&
                   input_[position_] <= '9') {
                exponent = exponent * 10 + input_[position_] - '0';
                if (exponent > 308) {
                    position_ = start;
                    return false;
                }
                ++position_;
            }
        }
        if (exponent != 0) {
            value *= std::pow(10.0, exponent_negative ? -exponent : exponent);
        }
        output = negative ? -value : value;
        return std::isfinite(output);
    }

    [[nodiscard]] bool skip_value(const std::size_t depth = 0) noexcept
    {
        if (depth > kMaximumJsonDepth) {
            return false;
        }
        switch (peek()) {
        case '{':
            if (!consume('{')) {
                return false;
            }
            if (peek() == '}') {
                return consume('}');
            }
            while (true) {
                if (string_impl(nullptr, 0) == StringResult::invalid || !consume(':') ||
                    !skip_value(depth + 1)) {
                    return false;
                }
                if (consume('}')) {
                    return true;
                }
                if (!consume(',')) {
                    return false;
                }
            }
        case '[':
            if (!consume('[')) {
                return false;
            }
            if (peek() == ']') {
                return consume(']');
            }
            while (true) {
                if (!skip_value(depth + 1)) {
                    return false;
                }
                if (consume(']')) {
                    return true;
                }
                if (!consume(',')) {
                    return false;
                }
            }
        case '"':
            return string_impl(nullptr, 0) != StringResult::invalid;
        case 't':
            return literal("true");
        case 'f':
            return literal("false");
        case 'n':
            return literal("null");
        default: {
            double ignored = 0.0;
            return number(ignored);
        }
        }
    }

  private:
    [[nodiscard]] bool take(const char expected) noexcept
    {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool literal(const std::string_view expected) noexcept
    {
        if (input_.substr(position_, expected.size()) != expected) {
            return false;
        }
        position_ += expected.size();
        return true;
    }

    [[nodiscard]] StringResult string_impl(char* output,
                                           const std::size_t capacity) noexcept
    {
        whitespace();
        if (!take('"')) {
            return StringResult::invalid;
        }
        std::size_t written = 0;
        bool overflow = false;
        while (position_ < input_.size()) {
            char value = input_[position_++];
            if (value == '"') {
                if (output != nullptr && capacity > 0) {
                    output[written < capacity ? written : capacity - 1] = '\0';
                }
                return overflow ? StringResult::overflow : StringResult::ok;
            }
            if (static_cast<unsigned char>(value) < 0x20U) {
                return StringResult::invalid;
            }
            if (value == '\\') {
                if (position_ >= input_.size()) {
                    return StringResult::invalid;
                }
                switch (input_[position_++]) {
                case '"':
                    value = '"';
                    break;
                case '\\':
                    value = '\\';
                    break;
                case '/':
                    value = '/';
                    break;
                case 'b':
                    value = '\b';
                    break;
                case 'f':
                    value = '\f';
                    break;
                case 'n':
                    value = '\n';
                    break;
                case 'r':
                    value = '\r';
                    break;
                case 't':
                    value = '\t';
                    break;
                default:
                    return StringResult::invalid;
                }
            }
            if (output != nullptr && written + 1 < capacity) {
                output[written] = value;
            }
            else if (output != nullptr) {
                overflow = true;
            }
            ++written;
        }
        return StringResult::invalid;
    }

    std::string_view input_;
    std::size_t position_{0};
};

struct ParseState {
    TrackDefinition definition{};
    bool capacity_error{false};
    bool schema{false};
    bool id{false};
    bool name{false};
    bool reference{false};
    bool geofence{false};
    bool gates{false};
    bool timing{false};
};

bool valid_point(const GeographicPoint& point) noexcept
{
    return std::isfinite(point.latitude_deg) && std::isfinite(point.longitude_deg) &&
           point.latitude_deg >= -90.0 && point.latitude_deg <= 90.0 &&
           point.longitude_deg >= -180.0 && point.longitude_deg <= 180.0;
}

bool parse_point(JsonReader& reader, GeographicPoint& point) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool latitude = false;
    bool longitude = false;
    while (reader.peek() != '}') {
        std::array<char, 32> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "lat_deg") == 0) {
            latitude = reader.number(point.latitude_deg);
            if (!latitude) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "lon_deg") == 0) {
            longitude = reader.number(point.longitude_deg);
            if (!longitude) {
                return false;
            }
        }
        else {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && latitude && longitude;
}

bool parse_geofence(JsonReader& reader, GeofenceDefinition& geofence) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool latitude = false;
    bool longitude = false;
    bool radius = false;
    while (reader.peek() != '}') {
        std::array<char, 32> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "center_lat_deg") == 0) {
            latitude = reader.number(geofence.center.latitude_deg);
            if (!latitude) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "center_lon_deg") == 0) {
            longitude = reader.number(geofence.center.longitude_deg);
            if (!longitude) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "radius_m") == 0) {
            radius = reader.number(geofence.radius_m);
            if (!radius) {
                return false;
            }
        }
        else {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && latitude && longitude && radius;
}

bool parse_gate(JsonReader& reader, DirectedGateDefinition& gate) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool left = false;
    bool right = false;
    bool heading = false;
    bool tolerance = false;
    bool minimum_speed = false;
    bool rearm_corridor = false;
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "left") == 0) {
            left = parse_point(reader, gate.left);
            if (!left) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "right") == 0) {
            right = parse_point(reader, gate.right);
            if (!right) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "direction_heading_deg") == 0) {
            heading = reader.number(gate.direction_heading_deg);
            if (!heading) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "heading_tolerance_deg") == 0) {
            tolerance = reader.number(gate.heading_tolerance_deg);
            if (!tolerance) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "minimum_crossing_speed_mps") == 0) {
            minimum_speed = reader.number(gate.minimum_crossing_speed_mps);
            if (!minimum_speed) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "rearm_corridor_m") == 0) {
            rearm_corridor = reader.number(gate.rearm_corridor_m);
            if (!rearm_corridor) {
                return false;
            }
        }
        else {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && left && right && heading && tolerance && minimum_speed &&
           rearm_corridor;
}

bool parse_gates(JsonReader& reader, CircuitGateDefinitions& gates) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool start = false;
    bool finish = false;
    bool pit_entry = false;
    bool pit_exit = false;
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "start") == 0) {
            start = parse_gate(reader, gates.start);
            if (!start) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "finish") == 0) {
            finish = parse_gate(reader, gates.finish);
            if (!finish) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "pit_entry") == 0) {
            pit_entry = parse_gate(reader, gates.pit_entry);
            if (!pit_entry) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "pit_exit") == 0) {
            pit_exit = parse_gate(reader, gates.pit_exit);
            if (!pit_exit) {
                return false;
            }
        }
        else {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && start && finish && pit_entry && pit_exit;
}

bool parse_timing(JsonReader& reader, double& minimum_lap_time_s) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool minimum_lap_time = false;
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "minimum_lap_time_s") == 0) {
            minimum_lap_time = reader.number(minimum_lap_time_s);
            if (!minimum_lap_time) {
                return false;
            }
        }
        else {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && minimum_lap_time;
}

bool parse_sectors(JsonReader& reader, ParseState& state) noexcept
{
    if (!reader.consume('[')) {
        return false;
    }
    std::size_t count = 0;
    if (reader.peek() == ']') {
        return reader.consume(']');
    }
    while (true) {
        if (++count > kMaximumSectorCount) {
            state.capacity_error = true;
            return false;
        }
        if (!reader.skip_value()) {
            return false;
        }
        if (reader.consume(']')) {
            state.definition.sector_count = static_cast<std::uint8_t>(count);
            return true;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
}

bool parse_root(JsonReader& reader, ParseState& state) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        const auto key_result = reader.string(key);
        if (key_result == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "schema_version") == 0) {
            double version = 0.0;
            if (!reader.number(version) || version < 0.0 || version > 65'535.0 ||
                std::floor(version) != version) {
                return false;
            }
            state.definition.schema_version = static_cast<std::uint16_t>(version);
            state.schema = true;
        }
        else if (std::strcmp(key.data(), "track_id") == 0) {
            const auto result = reader.string(state.definition.track_id);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            state.id = result == StringResult::ok && state.definition.track_id[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "name") == 0) {
            const auto result = reader.string(state.definition.name);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            state.name = result == StringResult::ok && state.definition.name[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "country") == 0) {
            const auto result = reader.string(state.definition.country);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "reference") == 0) {
            state.reference = parse_point(reader, state.definition.reference);
            if (!state.reference) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "geofence") == 0) {
            state.geofence = parse_geofence(reader, state.definition.geofence);
            if (!state.geofence) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "gates") == 0) {
            state.gates = parse_gates(reader, state.definition.gates);
            if (!state.gates) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "timing") == 0) {
            state.timing = parse_timing(reader, state.definition.minimum_lap_time_s);
            if (!state.timing) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "sectors") == 0) {
            if (!parse_sectors(reader, state)) {
                return false;
            }
        }
        else if (!reader.skip_value()) {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && reader.finished();
}

bool valid_identifier(const std::array<char, kTrackIdCapacity>& identifier) noexcept
{
    for (std::size_t index = 0; index < identifier.size() && identifier[index] != '\0'; ++index) {
        const char value = identifier[index];
        const bool alpha_numeric = (value >= 'a' && value <= 'z') ||
                                   (value >= 'A' && value <= 'Z') ||
                                   (value >= '0' && value <= '9');
        if (!alpha_numeric && value != '-' && value != '_' && value != '.') {
            return false;
        }
    }
    return identifier[0] != '\0';
}

LocalPoint project(const GeographicPoint& point, const GeographicPoint& reference) noexcept
{
    const auto latitude_delta = (point.latitude_deg - reference.latitude_deg) *
                                kDegreesToRadians;
    const auto longitude_delta = (point.longitude_deg - reference.longitude_deg) *
                                 kDegreesToRadians;
    return {longitude_delta * kEarthRadiusM *
                std::cos(reference.latitude_deg * kDegreesToRadians),
            latitude_delta * kEarthRadiusM};
}

TrackLoadReport report(const TrackLoadResult result, const JsonReader& reader) noexcept
{
    return {result, reader.position()};
}

}  // namespace

TrackLoadReport load_track_definition(const std::string_view json,
                                      TrackDefinition& output) noexcept
{
    if (json.empty()) {
        return {TrackLoadResult::empty, 0};
    }
    if (json.size() > kMaximumTrackFileBytes) {
        return {TrackLoadResult::file_too_large, kMaximumTrackFileBytes};
    }

    JsonReader reader{json};
    ParseState parsed{};
    if (!parse_root(reader, parsed)) {
        return report(parsed.capacity_error ? TrackLoadResult::capacity_exceeded
                                            : TrackLoadResult::invalid_json,
                      reader);
    }
    if (parsed.capacity_error) {
        return report(TrackLoadResult::capacity_exceeded, reader);
    }
    if (parsed.schema &&
        parsed.definition.schema_version != kCurrentTrackSchemaVersion) {
        return report(TrackLoadResult::unsupported_version, reader);
    }
    if (!parsed.schema || !parsed.id || !parsed.name || !parsed.reference ||
        !parsed.geofence || !parsed.gates || !parsed.timing) {
        return report(TrackLoadResult::missing_required_field, reader);
    }

    auto& definition = parsed.definition;
    const std::array<DirectedGateDefinition*, 4> gates{
        &definition.gates.start,
        &definition.gates.finish,
        &definition.gates.pit_entry,
        &definition.gates.pit_exit,
    };
    if (!valid_identifier(definition.track_id) || !valid_point(definition.reference) ||
        !valid_point(definition.geofence.center) ||
        !std::isfinite(definition.geofence.radius_m) ||
        definition.geofence.radius_m <= 0.0 ||
        !std::isfinite(definition.minimum_lap_time_s) ||
        definition.minimum_lap_time_s <= 0.0 || definition.minimum_lap_time_s > 3'600.0) {
        return report(TrackLoadResult::invalid_value, reader);
    }

    constexpr std::array<TrackLoadResult, 4> degenerate_results{
        TrackLoadResult::degenerate_start_gate,
        TrackLoadResult::degenerate_finish_gate,
        TrackLoadResult::degenerate_pit_entry_gate,
        TrackLoadResult::degenerate_pit_exit_gate,
    };
    for (std::size_t index = 0; index < gates.size(); ++index) {
        auto& gate = *gates[index];
        if (!valid_point(gate.left) || !valid_point(gate.right) ||
            !std::isfinite(gate.direction_heading_deg) || gate.direction_heading_deg < 0.0 ||
            gate.direction_heading_deg >= 360.0 ||
            !std::isfinite(gate.heading_tolerance_deg) || gate.heading_tolerance_deg < 0.0 ||
            gate.heading_tolerance_deg > 180.0 ||
            !std::isfinite(gate.minimum_crossing_speed_mps) ||
            gate.minimum_crossing_speed_mps <= 0.0 ||
            gate.minimum_crossing_speed_mps > 150.0 ||
            !std::isfinite(gate.rearm_corridor_m) || gate.rearm_corridor_m <= 0.0 ||
            gate.rearm_corridor_m > 1'000.0) {
            return report(TrackLoadResult::invalid_value, reader);
        }
        gate.local_left = project(gate.left, definition.reference);
        gate.local_right = project(gate.right, definition.reference);
        const auto east = gate.local_right.east_m - gate.local_left.east_m;
        const auto north = gate.local_right.north_m - gate.local_left.north_m;
        if (std::hypot(east, north) < 1.0) {
            return report(degenerate_results[index], reader);
        }
    }
    definition.definition_hash = hash_track_definition(json);
    output = definition;
    return report(TrackLoadResult::loaded, reader);
}

std::uint64_t hash_track_definition(const std::string_view bytes) noexcept
{
    std::uint64_t value = 14'695'981'039'346'656'037ULL;
    for (const auto byte : bytes) {
        value ^= static_cast<std::uint8_t>(byte);
        value *= 1'099'511'628'211ULL;
    }
    return value;
}

void format_definition_hash(const std::uint64_t hash,
                            std::array<char, 17>& output) noexcept
{
    std::snprintf(output.data(), output.size(), "%016llx",
                  static_cast<unsigned long long>(hash));
}

const char* track_load_result_name(const TrackLoadResult result) noexcept
{
    switch (result) {
    case TrackLoadResult::loaded:
        return "loaded";
    case TrackLoadResult::empty:
        return "empty";
    case TrackLoadResult::file_too_large:
        return "file-too-large";
    case TrackLoadResult::invalid_json:
        return "invalid-json";
    case TrackLoadResult::missing_required_field:
        return "missing-required-field";
    case TrackLoadResult::unsupported_version:
        return "unsupported-version";
    case TrackLoadResult::invalid_value:
        return "invalid-value";
    case TrackLoadResult::capacity_exceeded:
        return "capacity-exceeded";
    case TrackLoadResult::degenerate_start_gate:
        return "degenerate-start-gate";
    case TrackLoadResult::degenerate_finish_gate:
        return "degenerate-finish-gate";
    case TrackLoadResult::degenerate_pit_entry_gate:
        return "degenerate-pit-entry-gate";
    case TrackLoadResult::degenerate_pit_exit_gate:
        return "degenerate-pit-exit-gate";
    }
    return "invalid-json";
}

}  // namespace track_timer::track
