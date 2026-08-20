#include "track_timer/settings/settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace track_timer::settings {
namespace {

constexpr std::array<std::uint8_t, 4> kMagic{'T', 'S', 'T', 'G'};
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kLegacyV1PayloadSize = 7;
constexpr std::size_t kLegacyV2PayloadSize = 62;
constexpr std::size_t kLegacyV3PayloadSize = 63;
constexpr std::size_t kLegacyV4PayloadSize = 66;
// v5 appends the durations as seconds. The minutes still written at offsets 0-3 are
// vestigial: they keep the shared prefix that every earlier version's decoder reads, and
// v5 ignores them in favour of the appended values.
constexpr std::size_t kLegacyV5PayloadSize = 74;
// v6 appends the session trigger, which nothing before it had.
constexpr std::size_t kCurrentPayloadSize = 75;

inline constexpr std::uint32_t kMaximumDurationSeconds = 24U * 60U * 60U;
inline constexpr std::uint32_t kMinimumSessionSeconds = 60U;

bool valid_brightness(const std::uint8_t percent) noexcept
{
    return percent == 25 || percent == 50 || percent == 75 || percent == 100;
}

bool valid_launch_sensitivity(const std::uint16_t milli_g) noexcept
{
    constexpr std::array<std::uint16_t, 10> values{0, 500, 1'000, 1'250, 1'500,
                                                   1'750, 2'000, 2'500, 3'500, 4'000};
    return std::find(values.begin(), values.end(), milli_g) != values.end();
}

bool valid_track_identifier(
    const std::array<char, kTrackIdentifierCapacity>& identifier) noexcept
{
    bool terminated = false;
    for (const char character : identifier) {
        if (character == '\0') {
            terminated = true;
            continue;
        }
        if (terminated) {
            return false;
        }
        const auto value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '-' && character != '_' && character != '.') {
            return false;
        }
    }
    return terminated;
}

void put_u16(std::uint8_t* output, const std::uint16_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void put_u32(std::uint8_t* output, const std::uint32_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    output[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    output[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::uint16_t get_u16(const std::uint8_t* input) noexcept
{
    return static_cast<std::uint16_t>(input[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(input[1]) << 8U);
}

std::uint32_t get_u32(const std::uint8_t* input) noexcept
{
    return static_cast<std::uint32_t>(input[0]) |
           (static_cast<std::uint32_t>(input[1]) << 8U) |
           (static_cast<std::uint32_t>(input[2]) << 16U) |
           (static_cast<std::uint32_t>(input[3]) << 24U);
}

std::uint32_t checksum(const std::uint8_t* data, const std::size_t size) noexcept
{
    std::uint32_t value = 2'166'136'261U;
    for (std::size_t index = 0; index < size; ++index) {
        value ^= data[index];
        value *= 16'777'619U;
    }
    return value;
}

SettingsBlob make_blob(const std::uint16_t version, const std::uint8_t* payload,
                       const std::size_t payload_size) noexcept
{
    SettingsBlob blob{};
    if (payload_size > kSettingsBlobCapacity - kHeaderSize) {
        return blob;
    }
    std::copy(kMagic.begin(), kMagic.end(), blob.bytes.begin());
    put_u16(blob.bytes.data() + 4, version);
    put_u16(blob.bytes.data() + 6, static_cast<std::uint16_t>(payload_size));
    put_u32(blob.bytes.data() + 8, checksum(payload, payload_size));
    std::copy(payload, payload + payload_size, blob.bytes.begin() + kHeaderSize);
    blob.size = kHeaderSize + payload_size;
    return blob;
}

bool header_valid(const SettingsBlob& blob, std::uint16_t& version,
                  std::size_t& payload_size) noexcept
{
    if (blob.size < kHeaderSize || blob.size > blob.bytes.size() ||
        !std::equal(kMagic.begin(), kMagic.end(), blob.bytes.begin())) {
        return false;
    }
    version = get_u16(blob.bytes.data() + 4);
    payload_size = get_u16(blob.bytes.data() + 6);
    if (payload_size != blob.size - kHeaderSize) {
        return false;
    }
    return get_u32(blob.bytes.data() + 8) ==
           checksum(blob.bytes.data() + kHeaderSize, payload_size);
}

bool valid_legacy_settings(const LegacySettingsV1& settings) noexcept
{
    return settings.session_duration_seconds >= 1 &&
           settings.session_duration_seconds <= 24 * 60 &&
           settings.rest_duration_seconds <= 24 * 60 &&
           valid_brightness(settings.brightness_percent) &&
           settings.orientation <= OrientationMode::automatic;
}

}  // namespace

bool valid_settings(const DeviceSettings& settings) noexcept
{
    return settings.session_duration_seconds >= kMinimumSessionSeconds &&
           settings.session_duration_seconds <= kMaximumDurationSeconds &&
           settings.rest_duration_seconds <= kMaximumDurationSeconds &&
           valid_launch_sensitivity(settings.launch_sensitivity_milli_g) &&
           settings.average_lap_seconds <= 59 * 60 + 59 &&
           valid_brightness(settings.day_brightness_percent) &&
           valid_brightness(settings.night_brightness_percent) &&
           settings.operating_mode <= OperatingMode::g_meter &&
           settings.orientation <= OrientationMode::automatic &&
           settings.lower_display <= LowerDisplayMode::laps_remaining &&
           (settings.average_lap_seconds > 0 ||
            settings.lower_display == LowerDisplayMode::elapsed) &&
           settings.lap_boundary <= LapBoundaryMode::finish &&
           settings.session_trigger <= SessionTrigger::gps &&
           valid_track_identifier(settings.selected_track_id);
}

bool settings_equal(const DeviceSettings& left, const DeviceSettings& right) noexcept
{
    return left.session_duration_seconds == right.session_duration_seconds &&
           left.rest_duration_seconds == right.rest_duration_seconds &&
           left.launch_sensitivity_milli_g == right.launch_sensitivity_milli_g &&
           left.average_lap_seconds == right.average_lap_seconds &&
           left.day_brightness_percent == right.day_brightness_percent &&
           left.night_brightness_percent == right.night_brightness_percent &&
           left.operating_mode == right.operating_mode && left.orientation == right.orientation &&
           left.auto_dim_enabled == right.auto_dim_enabled &&
           left.lower_display == right.lower_display &&
           left.selected_track_id == right.selected_track_id &&
           left.trackday_mode_enabled == right.trackday_mode_enabled &&
           left.lap_boundary == right.lap_boundary &&
           left.pit_exit_auto_start_enabled == right.pit_exit_auto_start_enabled &&
           left.pit_entry_auto_stop_enabled == right.pit_entry_auto_stop_enabled;
}

SettingsBlob encode_legacy_settings_v2(const DeviceSettings& settings) noexcept
{
    if (!valid_settings(settings)) {
        return {};
    }

    std::array<std::uint8_t, kLegacyV2PayloadSize> payload{};
    // Older formats only ever held minutes, so a value with seconds in it necessarily
    // loses them here. v5 carries the exact seconds separately.
    put_u16(payload.data(),
            static_cast<std::uint16_t>(settings.session_duration_seconds / 60U));
    put_u16(payload.data() + 2,
            static_cast<std::uint16_t>(settings.rest_duration_seconds / 60U));
    put_u16(payload.data() + 4, settings.launch_sensitivity_milli_g);
    put_u16(payload.data() + 6, settings.average_lap_seconds);
    payload[8] = settings.day_brightness_percent;
    payload[9] = settings.night_brightness_percent;
    payload[10] = static_cast<std::uint8_t>(settings.operating_mode);
    payload[11] = static_cast<std::uint8_t>(settings.orientation);
    payload[12] = settings.auto_dim_enabled ? 1U : 0U;
    payload[13] = static_cast<std::uint8_t>(settings.lower_display);
    std::memcpy(payload.data() + 14, settings.selected_track_id.data(),
                settings.selected_track_id.size());
    return make_blob(2, payload.data(), payload.size());
}

SettingsBlob encode_legacy_settings_v3(const DeviceSettings& settings) noexcept
{
    const auto legacy = encode_legacy_settings_v2(settings);
    if (legacy.size == 0) {
        return {};
    }
    std::array<std::uint8_t, kLegacyV3PayloadSize> payload{};
    std::copy_n(legacy.bytes.data() + kHeaderSize, kLegacyV2PayloadSize, payload.data());
    payload[62] = settings.trackday_mode_enabled ? 1U : 0U;
    return make_blob(3, payload.data(), payload.size());
}

SettingsBlob encode_legacy_settings_v4(const DeviceSettings& settings) noexcept
{
    const auto legacy = encode_legacy_settings_v3(settings);
    if (legacy.size == 0) {
        return {};
    }
    std::array<std::uint8_t, kLegacyV4PayloadSize> payload{};
    std::copy_n(legacy.bytes.data() + kHeaderSize, kLegacyV3PayloadSize, payload.data());
    payload[63] = static_cast<std::uint8_t>(settings.lap_boundary);
    payload[64] = settings.pit_exit_auto_start_enabled ? 1U : 0U;
    payload[65] = settings.pit_entry_auto_stop_enabled ? 1U : 0U;
    return make_blob(4, payload.data(), payload.size());
}

SettingsBlob encode_legacy_settings_v5(const DeviceSettings& settings) noexcept
{
    const auto legacy = encode_legacy_settings_v4(settings);
    if (legacy.size == 0) {
        return {};
    }
    std::array<std::uint8_t, kLegacyV5PayloadSize> payload{};
    std::copy_n(legacy.bytes.data() + kHeaderSize, kLegacyV4PayloadSize, payload.data());
    put_u32(payload.data() + 66, settings.session_duration_seconds);
    put_u32(payload.data() + 70, settings.rest_duration_seconds);
    return make_blob(5, payload.data(), payload.size());
}

SettingsBlob encode_settings(const DeviceSettings& settings) noexcept
{
    const auto legacy = encode_legacy_settings_v5(settings);
    if (legacy.size == 0) {
        return {};
    }
    std::array<std::uint8_t, kCurrentPayloadSize> payload{};
    std::copy_n(legacy.bytes.data() + kHeaderSize, kLegacyV5PayloadSize, payload.data());
    payload[74] = static_cast<std::uint8_t>(settings.session_trigger);
    return make_blob(kCurrentSettingsVersion, payload.data(), payload.size());
}

SettingsBlob encode_legacy_settings_v1(const LegacySettingsV1& settings) noexcept
{
    if (!valid_legacy_settings(settings)) {
        return {};
    }
    std::array<std::uint8_t, kLegacyV1PayloadSize> payload{};
    put_u16(payload.data(), settings.session_duration_seconds);
    put_u16(payload.data() + 2, settings.rest_duration_seconds);
    payload[4] = settings.brightness_percent;
    payload[5] = static_cast<std::uint8_t>(settings.orientation);
    payload[6] = settings.auto_dim_enabled ? 1U : 0U;
    return make_blob(1, payload.data(), payload.size());
}

DecodeResult decode_settings(const SettingsBlob& blob, DeviceSettings& settings) noexcept
{
    std::uint16_t version = 0;
    std::size_t payload_size = 0;
    if (!header_valid(blob, version, payload_size)) {
        return DecodeResult::corrupt;
    }

    const auto* payload = blob.bytes.data() + kHeaderSize;
    DeviceSettings candidate{};
    if (version == 1) {
        if (payload_size != kLegacyV1PayloadSize) {
            return DecodeResult::corrupt;
        }
        candidate.session_duration_seconds = get_u16(payload) * 60U;
        candidate.rest_duration_seconds = get_u16(payload + 2) * 60U;
        candidate.day_brightness_percent = payload[4];
        candidate.orientation = static_cast<OrientationMode>(payload[5]);
        candidate.auto_dim_enabled = payload[6] != 0;
        if (payload[6] > 1 || !valid_settings(candidate)) {
            return DecodeResult::corrupt;
        }
        settings = candidate;
        return DecodeResult::migrated_v1;
    }
    if (version != 2 && version != 3 && version != 4 && version != 5 &&
        version != kCurrentSettingsVersion) {
        return DecodeResult::unsupported_version;
    }
    const auto expected_payload_size =
        version == 2   ? kLegacyV2PayloadSize
        : version == 3 ? kLegacyV3PayloadSize
        : version == 4 ? kLegacyV4PayloadSize
        : version == 5 ? kLegacyV5PayloadSize
                       : kCurrentPayloadSize;
    if (payload_size != expected_payload_size) {
        return DecodeResult::corrupt;
    }

    // Every format before v5 stored whole minutes, so a migrated session is exact.
    candidate.session_duration_seconds = get_u16(payload) * 60U;
    candidate.rest_duration_seconds = get_u16(payload + 2) * 60U;
    candidate.launch_sensitivity_milli_g = get_u16(payload + 4);
    candidate.average_lap_seconds = get_u16(payload + 6);
    candidate.day_brightness_percent = payload[8];
    candidate.night_brightness_percent = payload[9];
    candidate.operating_mode = static_cast<OperatingMode>(payload[10]);
    candidate.orientation = static_cast<OrientationMode>(payload[11]);
    candidate.auto_dim_enabled = payload[12] != 0;
    candidate.lower_display = static_cast<LowerDisplayMode>(payload[13]);
    std::memcpy(candidate.selected_track_id.data(), payload + 14,
                candidate.selected_track_id.size());
    if (version >= 3) {
        candidate.trackday_mode_enabled = payload[62] != 0;
    }
    if (version >= 4) {
        candidate.lap_boundary = static_cast<LapBoundaryMode>(payload[63]);
        candidate.pit_exit_auto_start_enabled = payload[64] != 0;
        candidate.pit_entry_auto_stop_enabled = payload[65] != 0;
    }
    if (version >= 5) {
        // The authoritative durations, which the vestigial minutes above cannot express.
        candidate.session_duration_seconds = get_u32(payload + 66);
        candidate.rest_duration_seconds = get_u32(payload + 70);
    }
    if (version == kCurrentSettingsVersion) {
        candidate.session_trigger = static_cast<SessionTrigger>(payload[74]);
    }
    // Everything before v6 predates the trigger, so it defaults to manual, which is what
    // those devices were doing.
    if (payload[12] > 1 ||
        (version >= 3 && payload[62] > 1) ||
        (version >= 4 &&
         (payload[63] > static_cast<std::uint8_t>(LapBoundaryMode::finish) ||
          payload[64] > 1 || payload[65] > 1)) ||
        (version == kCurrentSettingsVersion &&
         payload[74] > static_cast<std::uint8_t>(SessionTrigger::gps)) ||
        !valid_settings(candidate)) {
        return DecodeResult::corrupt;
    }
    settings = candidate;
    return version == 2   ? DecodeResult::migrated_v2
           : version == 3 ? DecodeResult::migrated_v3
           : version == 4 ? DecodeResult::migrated_v4
           : version == 5 ? DecodeResult::migrated_v5
                          : DecodeResult::current;
}

FeatureAvailability evaluate_features(const SubsystemSnapshot& subsystems) noexcept
{
    return FeatureAvailability{
        true,
        subsystems.gnss == SubsystemState::ready,
        subsystems.storage == SubsystemState::ready ||
            subsystems.storage == SubsystemState::degraded,
        subsystems.imu == SubsystemState::ready,
        subsystems.touch == SubsystemState::ready,
        subsystems.rtc == SubsystemState::ready || subsystems.rtc == SubsystemState::degraded,
    };
}

SettingsManager::SettingsManager(SettingsStore& store) noexcept : store_(store) {}

SettingsLoadReport SettingsManager::load() noexcept
{
    SettingsBlob blob{};
    const auto read_result = store_.read(blob);
    has_pending_change_ = false;
    pending_ = {};
    if (read_result == StoreReadResult::error) {
        current_ = {};
        return {SettingsSource::defaults_storage_error, false};
    }
    if (read_result == StoreReadResult::missing) {
        current_ = {};
        return {SettingsSource::defaults_missing, persist(current_)};
    }

    DeviceSettings decoded{};
    switch (decode_settings(blob, decoded)) {
    case DecodeResult::current:
        current_ = decoded;
        return {SettingsSource::current, true};
    case DecodeResult::migrated_v1:
        current_ = decoded;
        return {SettingsSource::migrated_v1, persist(current_)};
    case DecodeResult::migrated_v2:
        current_ = decoded;
        return {SettingsSource::migrated_v2, persist(current_)};
    case DecodeResult::migrated_v3:
        current_ = decoded;
        return {SettingsSource::migrated_v3, persist(current_)};
    case DecodeResult::migrated_v4:
        current_ = decoded;
        return {SettingsSource::migrated_v4, persist(current_)};
    case DecodeResult::migrated_v5:
        current_ = decoded;
        return {SettingsSource::migrated_v5, persist(current_)};
    case DecodeResult::corrupt:
        current_ = {};
        return {SettingsSource::defaults_corrupt, persist(current_)};
    case DecodeResult::unsupported_version:
        current_ = {};
        return {SettingsSource::defaults_unsupported, persist(current_)};
    }
    current_ = {};
    return {SettingsSource::defaults_corrupt, false};
}

SettingsApplyResult SettingsManager::apply(const DeviceSettings& settings,
                                           const bool session_active) noexcept
{
    if (!valid_settings(settings)) {
        return SettingsApplyResult::invalid_settings;
    }
    if (session_active) {
        pending_ = settings;
        has_pending_change_ = true;
        return SettingsApplyResult::deferred;
    }
    if (!persist(settings)) {
        return SettingsApplyResult::storage_error;
    }
    current_ = settings;
    pending_ = {};
    has_pending_change_ = false;
    return SettingsApplyResult::applied;
}

SettingsApplyResult SettingsManager::apply_deferred(const bool session_active) noexcept
{
    if (!has_pending_change_) {
        return SettingsApplyResult::no_pending_change;
    }
    if (session_active) {
        return SettingsApplyResult::deferred;
    }
    if (!persist(pending_)) {
        return SettingsApplyResult::storage_error;
    }
    current_ = pending_;
    pending_ = {};
    has_pending_change_ = false;
    return SettingsApplyResult::applied;
}

const DeviceSettings& SettingsManager::current() const noexcept
{
    return current_;
}

const DeviceSettings& SettingsManager::pending() const noexcept
{
    return pending_;
}

bool SettingsManager::has_pending_change() const noexcept
{
    return has_pending_change_;
}

bool SettingsManager::persist(const DeviceSettings& settings) noexcept
{
    const auto blob = encode_settings(settings);
    return blob.size > 0 && store_.write_atomic(blob);
}

}  // namespace track_timer::settings
