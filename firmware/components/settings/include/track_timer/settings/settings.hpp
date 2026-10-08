#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::settings {

inline constexpr std::uint16_t kCurrentSettingsVersion = 7;
inline constexpr std::size_t kSettingsBlobCapacity = 128;
inline constexpr std::size_t kTrackIdentifierCapacity = 48;

enum class OperatingMode : std::uint8_t {
    timer,
    g_meter,
    // Receiver diagnosis: road speed and fix data only. Appended rather than inserted so
    // every value already written to NVS keeps its meaning, which is why adding it needs
    // no format version bump and no migration - only a wider validation bound.
    gps_only,
};

enum class OrientationMode : std::uint8_t {
    fixed_0,
    fixed_90,
    fixed_180,
    fixed_270,
    automatic,
};

enum class LowerDisplayMode : std::uint8_t {
    elapsed,
    laps_remaining,
};

// What starts a session. Manual is the button; the other two arm the timer and wait.
enum class SessionTrigger : std::uint8_t {
    manual,
    imu,
    gps,
};

enum class LapBoundaryMode : std::uint8_t {
    start,
    finish,
};

// The IMU session trigger reads forward acceleration, which a car reaches at roughly 0.3
// to 1.0 g. The previous ladder ran to 4.0 g, so six of its ten choices could never fire,
// and it offered nothing below 0.5 g where a deliberate pit exit actually sits.
//
// One definition, consumed by the picker, the stepping editor and the validator alike.
// Three copies of it were how the values drifted away from anything measurable without
// anyone noticing.
inline constexpr std::array<std::uint16_t, 10> kLaunchSensitivityMilliG{
    150, 200, 250, 300, 350, 400, 500, 600, 800, 1'000};
// A deliberate pit exit, without tripping on a bump.
inline constexpr std::uint16_t kDefaultLaunchSensitivityMilliG = 300;

// Snaps any stored value onto the ladder. Settings written before v7 hold values that are
// no longer offered, and rejecting them would fail the whole blob and take every unrelated
// setting back to defaults with it.
[[nodiscard]] std::uint16_t nearest_launch_sensitivity(std::uint16_t milli_g) noexcept;
[[nodiscard]] bool valid_launch_sensitivity(std::uint16_t milli_g) noexcept;

struct DeviceSettings {
    // Seconds, not minutes: the roller sets these as minutes and seconds, and 24 hours of
    // seconds does not fit a uint16.
    std::uint32_t session_duration_seconds{20 * 60};
    std::uint32_t rest_duration_seconds{20 * 60};
    std::uint16_t launch_sensitivity_milli_g{kDefaultLaunchSensitivityMilliG};
    std::uint16_t average_lap_seconds{0};
    std::uint8_t day_brightness_percent{100};
    std::uint8_t night_brightness_percent{50};
    OperatingMode operating_mode{OperatingMode::timer};
    OrientationMode orientation{OrientationMode::fixed_0};
    bool auto_dim_enabled{false};
    LowerDisplayMode lower_display{LowerDisplayMode::elapsed};
    std::array<char, kTrackIdentifierCapacity> selected_track_id{};
    bool trackday_mode_enabled{false};
    LapBoundaryMode lap_boundary{LapBoundaryMode::finish};
    bool pit_exit_auto_start_enabled{false};
    bool pit_entry_auto_stop_enabled{false};
    // Appended, like every field before it: this struct is aggregate-initialised at call
    // sites. Owns pit_exit_auto_start_enabled, which is derived from it rather than
    // edited separately, so the two cannot disagree.
    SessionTrigger session_trigger{SessionTrigger::manual};
};

struct LegacySettingsV1 {
    std::uint16_t session_duration_seconds{20};
    std::uint16_t rest_duration_seconds{20};
    std::uint8_t brightness_percent{100};
    OrientationMode orientation{OrientationMode::fixed_0};
    bool auto_dim_enabled{false};
};

struct SettingsBlob {
    std::array<std::uint8_t, kSettingsBlobCapacity> bytes{};
    std::size_t size{0};
};

enum class StoreReadResult : std::uint8_t {
    found,
    missing,
    error,
};

class SettingsStore {
  public:
    virtual ~SettingsStore() = default;
    virtual StoreReadResult read(SettingsBlob& blob) noexcept = 0;
    virtual bool write_atomic(const SettingsBlob& blob) noexcept = 0;
};

enum class DecodeResult : std::uint8_t {
    current,
    migrated_v1,
    migrated_v2,
    migrated_v3,
    migrated_v4,
    migrated_v5,
    migrated_v6,
    corrupt,
    unsupported_version,
};

enum class SettingsSource : std::uint8_t {
    current,
    migrated_v1,
    migrated_v2,
    migrated_v3,
    migrated_v4,
    migrated_v5,
    migrated_v6,
    defaults_missing,
    defaults_corrupt,
    defaults_unsupported,
    defaults_storage_error,
};

struct SettingsLoadReport {
    SettingsSource source{SettingsSource::defaults_missing};
    bool current_format_persisted{false};
};

enum class SettingsApplyResult : std::uint8_t {
    applied,
    deferred,
    no_pending_change,
    invalid_settings,
    storage_error,
};

enum class SubsystemState : std::uint8_t {
    unavailable,
    degraded,
    ready,
};

struct SubsystemSnapshot {
    SubsystemState gnss{SubsystemState::unavailable};
    SubsystemState storage{SubsystemState::unavailable};
    SubsystemState imu{SubsystemState::unavailable};
    SubsystemState touch{SubsystemState::unavailable};
    SubsystemState rtc{SubsystemState::unavailable};
};

struct FeatureAvailability {
    bool session_timer{true};
    bool lap_timing{false};
    bool logging{false};
    bool g_meter{false};
    bool touch_control{false};
    bool wall_clock{false};
};

[[nodiscard]] bool valid_settings(const DeviceSettings& settings) noexcept;
[[nodiscard]] bool settings_equal(const DeviceSettings& left,
                                  const DeviceSettings& right) noexcept;
[[nodiscard]] SettingsBlob encode_settings(const DeviceSettings& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v1(
    const LegacySettingsV1& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v2(
    const DeviceSettings& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v6(
    const DeviceSettings& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v5(
    const DeviceSettings& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v4(
    const DeviceSettings& settings) noexcept;
[[nodiscard]] SettingsBlob encode_legacy_settings_v3(
    const DeviceSettings& settings) noexcept;
[[nodiscard]] DecodeResult decode_settings(const SettingsBlob& blob,
                                           DeviceSettings& settings) noexcept;
[[nodiscard]] FeatureAvailability evaluate_features(
    const SubsystemSnapshot& subsystems) noexcept;

class SettingsManager {
  public:
    explicit SettingsManager(SettingsStore& store) noexcept;

    [[nodiscard]] SettingsLoadReport load() noexcept;
    [[nodiscard]] SettingsApplyResult apply(const DeviceSettings& settings,
                                            bool session_active) noexcept;
    [[nodiscard]] SettingsApplyResult apply_deferred(bool session_active) noexcept;

    [[nodiscard]] const DeviceSettings& current() const noexcept;
    [[nodiscard]] const DeviceSettings& pending() const noexcept;
    [[nodiscard]] bool has_pending_change() const noexcept;

  private:
    [[nodiscard]] bool persist(const DeviceSettings& settings) noexcept;

    SettingsStore& store_;
    DeviceSettings current_{};
    DeviceSettings pending_{};
    bool has_pending_change_{false};
};

static_assert(std::is_trivially_copyable_v<DeviceSettings>);
static_assert(std::is_trivially_copyable_v<SubsystemSnapshot>);
static_assert(std::is_trivially_copyable_v<FeatureAvailability>);

}  // namespace track_timer::settings
