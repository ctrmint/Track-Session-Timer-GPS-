#include "track_timer/track/definition.hpp"
#include "track_timer/track/projection.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace track_timer::track {
namespace {

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
    TrackDefinitionField error_field{TrackDefinitionField::root};
    bool capacity_error{false};
    bool schema{false};
    bool revision{false};
    bool id{false};
    bool name{false};
    bool country{false};
    bool provenance{false};
    bool reference{false};
    bool geofence{false};
    bool gates{false};
    bool timing{false};
    bool sectors{false};
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

bool parse_provenance(JsonReader& reader, TrackProvenance& provenance,
                      bool& capacity_error) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool source = false;
    bool license = false;
    bool verified = false;
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        StringResult result{StringResult::invalid};
        if (std::strcmp(key.data(), "source") == 0 && !source) {
            result = reader.string(provenance.source);
            source = result == StringResult::ok && provenance.source[0] != '\0';
        }
        else if (std::strcmp(key.data(), "license") == 0 && !license) {
            result = reader.string(provenance.license);
            license = result == StringResult::ok && provenance.license[0] != '\0';
        }
        else if (std::strcmp(key.data(), "verified_utc") == 0 && !verified) {
            result = reader.string(provenance.verified_utc);
            verified = result == StringResult::ok && provenance.verified_utc[0] != '\0';
        }
        else {
            return false;
        }
        capacity_error = capacity_error || result == StringResult::overflow;
        if (result == StringResult::invalid) {
            return false;
        }
        if (reader.peek() == '}') {
            break;
        }
        if (!reader.consume(',')) {
            return false;
        }
    }
    return reader.consume('}') && source && license && verified;
}

bool parse_sector(JsonReader& reader, SectorDefinition& sector,
                  bool& capacity_error) noexcept
{
    if (!reader.consume('{')) {
        return false;
    }
    bool id = false;
    bool name = false;
    bool gate = false;
    while (reader.peek() != '}') {
        std::array<char, 40> key{};
        if (reader.string(key) == StringResult::invalid || !reader.consume(':')) {
            return false;
        }
        if (std::strcmp(key.data(), "sector_id") == 0 && !id) {
            const auto result = reader.string(sector.sector_id);
            capacity_error = capacity_error || result == StringResult::overflow;
            id = result == StringResult::ok && sector.sector_id[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "name") == 0 && !name) {
            const auto result = reader.string(sector.name);
            capacity_error = capacity_error || result == StringResult::overflow;
            name = result == StringResult::ok && sector.name[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "gate") == 0 && !gate) {
            gate = parse_gate(reader, sector.gate);
            if (!gate) {
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
    return reader.consume('}') && id && name && gate;
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
        if (!parse_sector(reader, state.definition.sectors[count - 1],
                          state.capacity_error)) {
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
            state.error_field = TrackDefinitionField::schema_version;
            double version = 0.0;
            if (state.schema || !reader.number(version) || version < 0.0 ||
                version > 65'535.0 ||
                std::floor(version) != version) {
                return false;
            }
            state.definition.schema_version = static_cast<std::uint16_t>(version);
            state.schema = true;
        }
        else if (std::strcmp(key.data(), "revision") == 0) {
            state.error_field = TrackDefinitionField::revision;
            double revision = 0.0;
            if (state.revision || !reader.number(revision) || revision < 1.0 ||
                revision > 4'294'967'295.0 || std::floor(revision) != revision) {
                return false;
            }
            state.definition.revision = static_cast<std::uint32_t>(revision);
            state.revision = true;
        }
        else if (std::strcmp(key.data(), "track_id") == 0) {
            state.error_field = TrackDefinitionField::track_id;
            if (state.id) {
                return false;
            }
            const auto result = reader.string(state.definition.track_id);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            state.id = result == StringResult::ok && state.definition.track_id[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "name") == 0) {
            state.error_field = TrackDefinitionField::name;
            if (state.name) {
                return false;
            }
            const auto result = reader.string(state.definition.name);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            state.name = result == StringResult::ok && state.definition.name[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "country") == 0) {
            state.error_field = TrackDefinitionField::country;
            if (state.country) {
                return false;
            }
            const auto result = reader.string(state.definition.country);
            state.capacity_error = state.capacity_error || result == StringResult::overflow;
            state.country = result == StringResult::ok && state.definition.country[0] != '\0';
            if (result == StringResult::invalid) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "provenance") == 0) {
            state.error_field = TrackDefinitionField::provenance;
            if (state.provenance) {
                return false;
            }
            state.provenance = parse_provenance(reader, state.definition.provenance,
                                                state.capacity_error);
            if (!state.provenance) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "reference") == 0) {
            state.error_field = TrackDefinitionField::reference;
            if (state.reference) {
                return false;
            }
            state.reference = parse_point(reader, state.definition.reference);
            if (!state.reference) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "geofence") == 0) {
            state.error_field = TrackDefinitionField::geofence;
            if (state.geofence) {
                return false;
            }
            state.geofence = parse_geofence(reader, state.definition.geofence);
            if (!state.geofence) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "gates") == 0) {
            state.error_field = TrackDefinitionField::gates_start;
            if (state.gates) {
                return false;
            }
            state.gates = parse_gates(reader, state.definition.gates);
            if (!state.gates) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "timing") == 0) {
            state.error_field = TrackDefinitionField::timing;
            if (state.timing) {
                return false;
            }
            state.timing = parse_timing(reader, state.definition.minimum_lap_time_s);
            if (!state.timing) {
                return false;
            }
        }
        else if (std::strcmp(key.data(), "sectors") == 0) {
            state.error_field = TrackDefinitionField::sectors;
            if (state.sectors) {
                return false;
            }
            if (!parse_sectors(reader, state)) {
                return false;
            }
            state.sectors = true;
        }
        else {
            state.error_field = TrackDefinitionField::root;
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

template <std::size_t Capacity>
bool valid_identifier(const std::array<char, Capacity>& identifier) noexcept
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

bool valid_country(const std::array<char, kCountryCapacity>& country) noexcept
{
    const auto length = std::strlen(country.data());
    if (length < 2 || length > 3) {
        return false;
    }
    for (std::size_t index = 0; index < length; ++index) {
        if (country[index] < 'A' || country[index] > 'Z') {
            return false;
        }
    }
    return true;
}

template <std::size_t Capacity>
bool has_non_whitespace(const std::array<char, Capacity>& value) noexcept
{
    for (const auto character : value) {
        if (character == '\0') {
            return false;
        }
        if (character != ' ' && character != '\t' && character != '\r' &&
            character != '\n') {
            return true;
        }
    }
    return false;
}

bool valid_verified_timestamp(
    const std::array<char, kProvenanceTimestampCapacity>& value) noexcept
{
    const auto length = std::strlen(value.data());
    return length >= 20 && value[4] == '-' && value[7] == '-' && value[10] == 'T' &&
           value[13] == ':' && value[16] == ':' && value[length - 1] == 'Z';
}

TrackLoadReport report(const TrackLoadResult result, const JsonReader& reader,
                       const TrackDefinitionField field =
                           TrackDefinitionField::none) noexcept
{
    return {result, reader.position(), field};
}

TrackDefinitionField first_missing_field(const ParseState& state) noexcept
{
    if (!state.schema) {
        return TrackDefinitionField::schema_version;
    }
    if (!state.revision) {
        return TrackDefinitionField::revision;
    }
    if (!state.id) {
        return TrackDefinitionField::track_id;
    }
    if (!state.name) {
        return TrackDefinitionField::name;
    }
    if (!state.country) {
        return TrackDefinitionField::country;
    }
    if (!state.provenance) {
        return TrackDefinitionField::provenance;
    }
    if (!state.reference) {
        return TrackDefinitionField::reference;
    }
    if (!state.geofence) {
        return TrackDefinitionField::geofence;
    }
    if (!state.gates) {
        return TrackDefinitionField::gates_start;
    }
    if (!state.timing) {
        return TrackDefinitionField::timing;
    }
    return TrackDefinitionField::sectors;
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
                      reader, parsed.error_field);
    }
    if (parsed.capacity_error) {
        return report(TrackLoadResult::capacity_exceeded, reader,
                      parsed.error_field);
    }
    if (parsed.schema &&
        parsed.definition.schema_version != kCurrentTrackSchemaVersion) {
        return report(TrackLoadResult::unsupported_version, reader,
                      TrackDefinitionField::schema_version);
    }
    if (!parsed.schema || !parsed.revision || !parsed.id || !parsed.name ||
        !parsed.country || !parsed.provenance || !parsed.reference ||
        !parsed.geofence || !parsed.gates || !parsed.timing || !parsed.sectors) {
        return report(TrackLoadResult::missing_required_field, reader,
                      first_missing_field(parsed));
    }

    auto& definition = parsed.definition;
    const std::array<DirectedGateDefinition*, 4> gates{
        &definition.gates.start,
        &definition.gates.finish,
        &definition.gates.pit_entry,
        &definition.gates.pit_exit,
    };
    if (!valid_identifier(definition.track_id)) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::track_id);
    }
    if (!has_non_whitespace(definition.name)) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::name);
    }
    if (!valid_country(definition.country)) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::country);
    }
    if (definition.revision == 0) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::revision);
    }
    if (!has_non_whitespace(definition.provenance.source) ||
        !has_non_whitespace(definition.provenance.license) ||
        !valid_verified_timestamp(definition.provenance.verified_utc)) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::provenance);
    }
    if (!valid_point(definition.reference)) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::reference);
    }
    if (!valid_point(definition.geofence.center) ||
        !std::isfinite(definition.geofence.radius_m) ||
        definition.geofence.radius_m <= 0.0 ||
        definition.geofence.radius_m > kMaximumCircuitProjectionRadiusM) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::geofence);
    }
    if (!std::isfinite(definition.minimum_lap_time_s) ||
        definition.minimum_lap_time_s <= 0.0 ||
        definition.minimum_lap_time_s > 3'600.0) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::timing);
    }

    CircuitProjection projection{};
    if (configure_circuit_projection(definition.reference, projection) !=
        ProjectionResult::projected) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::reference);
    }

    LocalPoint geofence_center{};
    if (project_to_circuit_local(projection, definition.geofence.center,
                                 geofence_center) != ProjectionResult::projected) {
        return report(TrackLoadResult::invalid_value, reader,
                      TrackDefinitionField::geofence);
    }
    if (std::hypot(geofence_center.east_m, geofence_center.north_m) >
        definition.geofence.radius_m) {
        return report(TrackLoadResult::geometry_outside_geofence, reader,
                      TrackDefinitionField::geofence);
    }

    constexpr std::array<TrackLoadResult, 4> degenerate_results{
        TrackLoadResult::degenerate_start_gate,
        TrackLoadResult::degenerate_finish_gate,
        TrackLoadResult::degenerate_pit_entry_gate,
        TrackLoadResult::degenerate_pit_exit_gate,
    };
    constexpr std::array<TrackDefinitionField, 4> gate_fields{
        TrackDefinitionField::gates_start,
        TrackDefinitionField::gates_finish,
        TrackDefinitionField::gates_pit_entry,
        TrackDefinitionField::gates_pit_exit,
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
            return report(TrackLoadResult::invalid_value, reader,
                          gate_fields[index]);
        }
        if (project_to_circuit_local(projection, gate.left, gate.local_left) !=
                ProjectionResult::projected ||
            project_to_circuit_local(projection, gate.right, gate.local_right) !=
                ProjectionResult::projected) {
            return report(TrackLoadResult::invalid_value, reader,
                          gate_fields[index]);
        }
        const auto east = gate.local_right.east_m - gate.local_left.east_m;
        const auto north = gate.local_right.north_m - gate.local_left.north_m;
        const auto gate_length_m = std::hypot(east, north);
        if (gate_length_m < 1.0) {
            return report(degenerate_results[index], reader,
                          gate_fields[index]);
        }
        constexpr double kPi = 3.14159265358979323846;
        const auto heading_rad = gate.direction_heading_deg * kPi / 180.0;
        const auto crossing_sine =
            std::abs(east * std::cos(heading_rad) -
                     north * std::sin(heading_rad)) /
            gate_length_m;
        if (gate_length_m > 1'000.0 ||
            crossing_sine < std::sin(10.0 * kPi / 180.0)) {
            return report(TrackLoadResult::invalid_value, reader,
                          gate_fields[index]);
        }
        const auto left_from_center =
            std::hypot(gate.local_left.east_m - geofence_center.east_m,
                       gate.local_left.north_m - geofence_center.north_m);
        const auto right_from_center =
            std::hypot(gate.local_right.east_m - geofence_center.east_m,
                       gate.local_right.north_m - geofence_center.north_m);
        if (left_from_center > definition.geofence.radius_m ||
            right_from_center > definition.geofence.radius_m) {
            return report(TrackLoadResult::geometry_outside_geofence, reader,
                          gate_fields[index]);
        }
    }

    for (std::size_t index = 0; index < definition.sector_count; ++index) {
        auto& sector = definition.sectors[index];
        if (!valid_identifier(sector.sector_id) || !has_non_whitespace(sector.name)) {
            return report(TrackLoadResult::invalid_value, reader,
                          TrackDefinitionField::sectors);
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (std::strcmp(sector.sector_id.data(),
                            definition.sectors[earlier].sector_id.data()) == 0) {
                return report(TrackLoadResult::duplicate_sector_id, reader,
                              TrackDefinitionField::sectors);
            }
        }
        auto& gate = sector.gate;
        if (!valid_point(gate.left) || !valid_point(gate.right) ||
            !std::isfinite(gate.direction_heading_deg) ||
            gate.direction_heading_deg < 0.0 || gate.direction_heading_deg >= 360.0 ||
            !std::isfinite(gate.heading_tolerance_deg) ||
            gate.heading_tolerance_deg < 0.0 || gate.heading_tolerance_deg > 180.0 ||
            !std::isfinite(gate.minimum_crossing_speed_mps) ||
            gate.minimum_crossing_speed_mps <= 0.0 ||
            gate.minimum_crossing_speed_mps > 150.0 ||
            !std::isfinite(gate.rearm_corridor_m) || gate.rearm_corridor_m <= 0.0 ||
            gate.rearm_corridor_m > 1'000.0 ||
            project_to_circuit_local(projection, gate.left, gate.local_left) !=
                ProjectionResult::projected ||
            project_to_circuit_local(projection, gate.right, gate.local_right) !=
                ProjectionResult::projected) {
            return report(TrackLoadResult::invalid_value, reader,
                          TrackDefinitionField::sectors);
        }
        const auto east = gate.local_right.east_m - gate.local_left.east_m;
        const auto north = gate.local_right.north_m - gate.local_left.north_m;
        const auto gate_length_m = std::hypot(east, north);
        if (gate_length_m < 1.0) {
            return report(TrackLoadResult::degenerate_sector_gate, reader,
                          TrackDefinitionField::sectors);
        }
        constexpr double kPi = 3.14159265358979323846;
        const auto heading_rad = gate.direction_heading_deg * kPi / 180.0;
        const auto crossing_sine =
            std::abs(east * std::cos(heading_rad) -
                     north * std::sin(heading_rad)) /
            gate_length_m;
        if (gate_length_m > 1'000.0 ||
            crossing_sine < std::sin(10.0 * kPi / 180.0)) {
            return report(TrackLoadResult::invalid_value, reader,
                          TrackDefinitionField::sectors);
        }
        const auto left_from_center =
            std::hypot(gate.local_left.east_m - geofence_center.east_m,
                       gate.local_left.north_m - geofence_center.north_m);
        const auto right_from_center =
            std::hypot(gate.local_right.east_m - geofence_center.east_m,
                       gate.local_right.north_m - geofence_center.north_m);
        if (left_from_center > definition.geofence.radius_m ||
            right_from_center > definition.geofence.radius_m) {
            return report(TrackLoadResult::geometry_outside_geofence, reader,
                          TrackDefinitionField::sectors);
        }
    }
    definition.definition_hash = hash_track_definition(json);
    output = definition;
    return report(TrackLoadResult::loaded, reader);
}

namespace {

class JsonWriter {
  public:
    explicit JsonWriter(TrackDefinitionBlob& output) noexcept : output_(output)
    {
        output_.bytes.fill('\0');
        output_.size = 0;
    }

    bool text(const char* value) noexcept
    {
        return append(value, std::strlen(value));
    }

    bool quoted(const char* value) noexcept
    {
        if (!text("\"")) {
            return false;
        }
        for (const auto* current = value; *current != '\0'; ++current) {
            switch (*current) {
            case '\"':
                if (!text("\\\"")) {
                    return false;
                }
                break;
            case '\\':
                if (!text("\\\\")) {
                    return false;
                }
                break;
            case '\n':
                if (!text("\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!text("\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!text("\\t")) {
                    return false;
                }
                break;
            default:
                if (static_cast<unsigned char>(*current) < 0x20U ||
                    !append(current, 1)) {
                    return false;
                }
            }
        }
        return text("\"");
    }

    bool format(const char* format, ...) noexcept
    {
        std::array<char, 256> formatted{};
        va_list arguments;
        va_start(arguments, format);
        const auto length = std::vsnprintf(formatted.data(), formatted.size(), format,
                                           arguments);
        va_end(arguments);
        return length >= 0 && static_cast<std::size_t>(length) < formatted.size() &&
               append(formatted.data(), static_cast<std::size_t>(length));
    }

  private:
    bool append(const char* value, const std::size_t length) noexcept
    {
        if (length > output_.bytes.size() - output_.size - 1) {
            failed_ = true;
            return false;
        }
        std::memcpy(output_.bytes.data() + output_.size, value, length);
        output_.size += length;
        output_.bytes[output_.size] = '\0';
        return !failed_;
    }

    TrackDefinitionBlob& output_;
    bool failed_{false};
};

bool write_point(JsonWriter& writer, const GeographicPoint& point) noexcept
{
    return writer.format("{\"lat_deg\":%.15g,\"lon_deg\":%.15g}",
                         point.latitude_deg, point.longitude_deg);
}

bool write_gate(JsonWriter& writer, const DirectedGateDefinition& gate) noexcept
{
    return writer.text("{\"left\":") && write_point(writer, gate.left) &&
           writer.text(",\"right\":") && write_point(writer, gate.right) &&
           writer.format(",\"direction_heading_deg\":%.15g,"
                         "\"heading_tolerance_deg\":%.15g,"
                         "\"minimum_crossing_speed_mps\":%.15g,"
                         "\"rearm_corridor_m\":%.15g}",
                         gate.direction_heading_deg, gate.heading_tolerance_deg,
                         gate.minimum_crossing_speed_mps, gate.rearm_corridor_m);
}

}  // namespace

TrackSerializeResult serialize_track_definition(
    const TrackDefinition& definition, TrackDefinitionBlob& output) noexcept
{
    TrackDefinitionBlob candidate{};
    JsonWriter writer{candidate};
    bool ok = writer.format("{\"schema_version\":%u,\"revision\":%u,\"track_id\":",
                            static_cast<unsigned>(definition.schema_version),
                            static_cast<unsigned>(definition.revision)) &&
              writer.quoted(definition.track_id.data()) && writer.text(",\"name\":") &&
              writer.quoted(definition.name.data()) && writer.text(",\"country\":") &&
              writer.quoted(definition.country.data()) &&
              writer.text(",\"provenance\":{\"source\":") &&
              writer.quoted(definition.provenance.source.data()) &&
              writer.text(",\"license\":") &&
              writer.quoted(definition.provenance.license.data()) &&
              writer.text(",\"verified_utc\":") &&
              writer.quoted(definition.provenance.verified_utc.data()) &&
              writer.text("},\"reference\":") && write_point(writer, definition.reference) &&
              writer.format(",\"geofence\":{\"center_lat_deg\":%.15g,"
                            "\"center_lon_deg\":%.15g,\"radius_m\":%.15g},"
                            "\"gates\":{\"start\":",
                            definition.geofence.center.latitude_deg,
                            definition.geofence.center.longitude_deg,
                            definition.geofence.radius_m) &&
              write_gate(writer, definition.gates.start) &&
              writer.text(",\"finish\":") && write_gate(writer, definition.gates.finish) &&
              writer.text(",\"pit_entry\":") &&
              write_gate(writer, definition.gates.pit_entry) &&
              writer.text(",\"pit_exit\":") && write_gate(writer, definition.gates.pit_exit) &&
              writer.format("},\"timing\":{\"minimum_lap_time_s\":%.15g},\"sectors\":[",
                            definition.minimum_lap_time_s);
    for (std::size_t index = 0; ok && index < definition.sector_count; ++index) {
        const auto& sector = definition.sectors[index];
        ok = (index == 0 || writer.text(",")) &&
             writer.text("{\"sector_id\":") && writer.quoted(sector.sector_id.data()) &&
             writer.text(",\"name\":") && writer.quoted(sector.name.data()) &&
             writer.text(",\"gate\":") && write_gate(writer, sector.gate) &&
             writer.text("}");
    }
    ok = ok && writer.text("]}");
    if (!ok) {
        return TrackSerializeResult::capacity_exceeded;
    }
    TrackDefinition round_trip{};
    if (load_track_definition(
            {candidate.bytes.data(), candidate.size}, round_trip).result !=
        TrackLoadResult::loaded) {
        return TrackSerializeResult::invalid_definition;
    }
    output = candidate;
    return TrackSerializeResult::serialized;
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
    case TrackLoadResult::degenerate_sector_gate:
        return "degenerate-sector-gate";
    case TrackLoadResult::duplicate_sector_id:
        return "duplicate-sector-id";
    case TrackLoadResult::geometry_outside_geofence:
        return "geometry-outside-geofence";
    }
    return "invalid-json";
}

const char* track_definition_field_name(const TrackDefinitionField field) noexcept
{
    switch (field) {
    case TrackDefinitionField::none:
        return "none";
    case TrackDefinitionField::root:
        return "<root>";
    case TrackDefinitionField::schema_version:
        return "schema_version";
    case TrackDefinitionField::revision:
        return "revision";
    case TrackDefinitionField::track_id:
        return "track_id";
    case TrackDefinitionField::name:
        return "name";
    case TrackDefinitionField::country:
        return "country";
    case TrackDefinitionField::provenance:
        return "provenance";
    case TrackDefinitionField::reference:
        return "reference";
    case TrackDefinitionField::geofence:
        return "geofence";
    case TrackDefinitionField::gates_start:
        return "gates.start";
    case TrackDefinitionField::gates_finish:
        return "gates.finish";
    case TrackDefinitionField::gates_pit_entry:
        return "gates.pit_entry";
    case TrackDefinitionField::gates_pit_exit:
        return "gates.pit_exit";
    case TrackDefinitionField::timing:
        return "timing";
    case TrackDefinitionField::sectors:
        return "sectors";
    }
    return "<root>";
}

}  // namespace track_timer::track
