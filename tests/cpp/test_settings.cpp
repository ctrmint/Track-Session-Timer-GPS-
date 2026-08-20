#include "track_timer/settings/settings.hpp"
#include "track_timer/simulator/file_settings_store.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace {

using namespace track_timer::settings;

class MemorySettingsStore final : public SettingsStore {
  public:
    StoreReadResult read(SettingsBlob& output) noexcept override
    {
        if (read_error) {
            return StoreReadResult::error;
        }
        if (!found) {
            return StoreReadResult::missing;
        }
        output = blob;
        return StoreReadResult::found;
    }

    bool write_atomic(const SettingsBlob& value) noexcept override
    {
        ++write_count;
        if (write_error) {
            return false;
        }
        blob = value;
        found = true;
        return true;
    }

    SettingsBlob blob{};
    std::size_t write_count{0};
    bool found{false};
    bool read_error{false};
    bool write_error{false};
};

DeviceSettings customized_settings()
{
    DeviceSettings settings{};
    settings.session_duration_seconds = 30 * 60 + 30;  // exercises the seconds v5 added
    settings.rest_duration_seconds = 10 * 60;
    settings.launch_sensitivity_milli_g = 600;
    settings.average_lap_seconds = 103;
    settings.day_brightness_percent = 75;
    settings.night_brightness_percent = 25;
    settings.operating_mode = OperatingMode::g_meter;
    settings.orientation = OrientationMode::automatic;
    settings.auto_dim_enabled = true;
    settings.lower_display = LowerDisplayMode::laps_remaining;
    settings.trackday_mode_enabled = true;
    settings.lap_boundary = LapBoundaryMode::start;
    settings.pit_exit_auto_start_enabled = true;
    settings.pit_entry_auto_stop_enabled = true;
    settings.session_trigger = SessionTrigger::imu;
    std::strcpy(settings.selected_track_id.data(), "synthetic-test-loop");
    return settings;
}

void test_validation_and_codec()
{
    const auto settings = customized_settings();
    assert(valid_settings(settings));
    const auto blob = encode_settings(settings);
    assert(blob.size > 0 && blob.size <= blob.bytes.size());
    DeviceSettings decoded{};
    assert(decode_settings(blob, decoded) == DecodeResult::current);
    assert(settings_equal(settings, decoded));

    auto invalid = settings;
    invalid.session_duration_seconds = 0;
    assert(!valid_settings(invalid));
    invalid = settings;
    invalid.launch_sensitivity_milli_g = 777;
    assert(!valid_settings(invalid));
    invalid = settings;
    invalid.average_lap_seconds = 0;
    assert(!valid_settings(invalid));
    invalid = settings;
    invalid.session_trigger = static_cast<SessionTrigger>(9);
    assert(!valid_settings(invalid));
    invalid = settings;
    invalid.lap_boundary = static_cast<LapBoundaryMode>(255);
    assert(!valid_settings(invalid));
    assert(encode_settings(invalid).size == 0);
}

// The ladder changed under stored settings, and rejecting a value no longer offered would
// fail the whole blob and take every unrelated setting back to defaults with it.
void test_launch_sensitivity_snapping()
{
    // Every value the old ladder offered maps onto the new one rather than being refused.
    for (const std::uint16_t old_value : {500, 1'000, 1'250, 1'500, 1'750, 2'000, 2'500,
                                          3'500, 4'000}) {
        const auto snapped = nearest_launch_sensitivity(old_value);
        assert(valid_launch_sensitivity(snapped));
    }

    // Everything above the top of the new ladder lands on it rather than somewhere odd.
    assert(nearest_launch_sensitivity(4'000) == 1'000);
    assert(nearest_launch_sensitivity(2'500) == 1'000);
    assert(nearest_launch_sensitivity(1'250) == 1'000);

    // Zero meant "off", which as a trigger threshold is a device that never starts. The
    // trigger is the on/off now, so off becomes the default rather than the lowest value,
    // which would be the most trigger-happy setting of all.
    assert(nearest_launch_sensitivity(0) == kDefaultLaunchSensitivityMilliG);
    assert(nearest_launch_sensitivity(0) != kLaunchSensitivityMilliG.front());

    // Nearest really is nearest, either side.
    assert(nearest_launch_sensitivity(160) == 150);
    assert(nearest_launch_sensitivity(190) == 200);
    assert(nearest_launch_sensitivity(700) == 600);
    assert(nearest_launch_sensitivity(750) == 800);

    // A value already on the ladder is left exactly alone.
    for (const auto value : kLaunchSensitivityMilliG) {
        assert(nearest_launch_sensitivity(value) == value);
    }

    // The ladder itself has to be reachable as forward acceleration, which is the defect
    // this replaced: a car manages roughly 0.3 to 1.0 g, and the old top was 4.0.
    assert(kLaunchSensitivityMilliG.front() < 500);   // below a deliberate pit exit
    assert(kLaunchSensitivityMilliG.back() <= 1'000); // within what a car can pull
    assert(valid_launch_sensitivity(kDefaultLaunchSensitivityMilliG));
}

void test_defaults_restart_and_deferred_apply()
{
    MemorySettingsStore store;
    SettingsManager first{store};
    const auto defaults = first.load();
    assert(defaults.source == SettingsSource::defaults_missing);
    assert(defaults.current_format_persisted);
    assert(store.write_count == 1);

    SettingsManager restarted{store};
    const auto restored = restarted.load();
    assert(restored.source == SettingsSource::current);
    assert(settings_equal(restarted.current(), DeviceSettings{}));

    const auto customized = customized_settings();
    assert(restarted.apply(customized, true) == SettingsApplyResult::deferred);
    assert(restarted.has_pending_change());
    assert(settings_equal(restarted.current(), DeviceSettings{}));
    assert(restarted.apply_deferred(true) == SettingsApplyResult::deferred);
    assert(restarted.apply_deferred(false) == SettingsApplyResult::applied);
    assert(settings_equal(restarted.current(), customized));

    SettingsManager second_restart{store};
    assert(second_restart.load().source == SettingsSource::current);
    assert(settings_equal(second_restart.current(), customized));
}

void test_migration_corruption_and_storage_errors()
{
    MemorySettingsStore legacy_store;
    legacy_store.found = true;
    legacy_store.blob = encode_legacy_settings_v1(
        LegacySettingsV1{25, 5, 75, OrientationMode::fixed_90, true});
    SettingsManager migrated{legacy_store};
    const auto migration = migrated.load();
    assert(migration.source == SettingsSource::migrated_v1);
    assert(migration.current_format_persisted);
    // v1 held whole minutes, so the migrated value is exact.
    assert(migrated.current().session_duration_seconds == 25 * 60);
    assert(migrated.current().rest_duration_seconds == 5 * 60);
    assert(migrated.current().day_brightness_percent == 75);
    assert(migrated.current().night_brightness_percent == 50);
    assert(migrated.current().orientation == OrientationMode::fixed_90);
    assert(!migrated.current().trackday_mode_enabled);
    assert(migrated.current().lap_boundary == LapBoundaryMode::finish);
    assert(!migrated.current().pit_exit_auto_start_enabled);
    assert(!migrated.current().pit_entry_auto_stop_enabled);

    MemorySettingsStore version_two_store;
    version_two_store.found = true;
    version_two_store.blob = encode_legacy_settings_v2(customized_settings());
    SettingsManager version_two{version_two_store};
    const auto version_two_migration = version_two.load();
    assert(version_two_migration.source == SettingsSource::migrated_v2);
    assert(version_two_migration.current_format_persisted);
    assert(!version_two.current().trackday_mode_enabled);
    assert(version_two.current().average_lap_seconds == 103);
    DeviceSettings migrated_v2{};
    assert(decode_settings(version_two_store.blob, migrated_v2) == DecodeResult::current);
    assert(!migrated_v2.trackday_mode_enabled);
    assert(migrated_v2.lap_boundary == LapBoundaryMode::finish);

    MemorySettingsStore version_three_store;
    version_three_store.found = true;
    version_three_store.blob = encode_legacy_settings_v3(customized_settings());
    SettingsManager version_three{version_three_store};
    const auto version_three_migration = version_three.load();
    assert(version_three_migration.source == SettingsSource::migrated_v3);
    assert(version_three_migration.current_format_persisted);
    assert(version_three.current().trackday_mode_enabled);
    assert(version_three.current().lap_boundary == LapBoundaryMode::finish);
    assert(!version_three.current().pit_exit_auto_start_enabled);
    assert(!version_three.current().pit_entry_auto_stop_enabled);

    MemorySettingsStore version_four_store;
    version_four_store.found = true;
    version_four_store.blob = encode_legacy_settings_v4(customized_settings());
    SettingsManager version_four{version_four_store};
    const auto version_four_migration = version_four.load();
    assert(version_four_migration.source == SettingsSource::migrated_v4);
    assert(version_four_migration.current_format_persisted);
    // v4 stored minutes, so the seconds in the source value cannot have survived it: 30:30
    // comes back as 30:00. Everything v4 could express is preserved exactly.
    assert(version_four.current().session_duration_seconds == 30 * 60);
    assert(version_four.current().rest_duration_seconds == 10 * 60);
    assert(version_four.current().trackday_mode_enabled);
    assert(version_four.current().pit_exit_auto_start_enabled);
    assert(version_four.current().average_lap_seconds == 103);

    MemorySettingsStore version_five_store;
    version_five_store.found = true;
    version_five_store.blob = encode_legacy_settings_v5(customized_settings());
    SettingsManager version_five{version_five_store};
    const auto version_five_migration = version_five.load();
    assert(version_five_migration.source == SettingsSource::migrated_v5);
    assert(version_five_migration.current_format_persisted);
    // v5 held the durations exactly, seconds and all.
    assert(version_five.current().session_duration_seconds == 30 * 60 + 30);
    // Nothing before v6 had a trigger, so it defaults to manual, which is what those
    // devices were doing.
    assert(version_five.current().session_trigger == SessionTrigger::manual);

    MemorySettingsStore version_six_store;
    version_six_store.found = true;
    version_six_store.blob = encode_legacy_settings_v6(customized_settings());
    SettingsManager version_six{version_six_store};
    const auto version_six_migration = version_six.load();
    assert(version_six_migration.source == SettingsSource::migrated_v6);
    assert(version_six_migration.current_format_persisted);
    // v6 carried the trigger, so it survives; the launch value was already on the new
    // ladder, so it is untouched.
    assert(version_six.current().session_trigger == SessionTrigger::imu);
    assert(version_six.current().launch_sensitivity_milli_g == 600);
    assert(version_six.current().session_duration_seconds == 30 * 60 + 30);

    MemorySettingsStore corrupt_store;
    corrupt_store.found = true;
    corrupt_store.blob = encode_settings(customized_settings());
    corrupt_store.blob.bytes[20] ^= 0x55U;
    SettingsManager corrupt{corrupt_store};
    const auto recovered = corrupt.load();
    assert(recovered.source == SettingsSource::defaults_corrupt);
    assert(recovered.current_format_persisted);
    DeviceSettings decoded{};
    assert(decode_settings(corrupt_store.blob, decoded) == DecodeResult::current);
    assert(settings_equal(decoded, DeviceSettings{}));

    MemorySettingsStore unsupported_store;
    unsupported_store.found = true;
    unsupported_store.blob = encode_settings(customized_settings());
    unsupported_store.blob.bytes[4] = 99;
    unsupported_store.blob.bytes[5] = 0;
    SettingsManager unsupported{unsupported_store};
    assert(unsupported.load().source == SettingsSource::defaults_unsupported);

    MemorySettingsStore failing_store;
    failing_store.read_error = true;
    SettingsManager read_failure{failing_store};
    const auto failure = read_failure.load();
    assert(failure.source == SettingsSource::defaults_storage_error);
    assert(!failure.current_format_persisted);

    failing_store.read_error = false;
    failing_store.write_error = true;
    assert(read_failure.apply(customized_settings(), false) ==
           SettingsApplyResult::storage_error);
    assert(settings_equal(read_failure.current(), DeviceSettings{}));
}

void test_degraded_feature_matrix()
{
    const auto unavailable = evaluate_features(SubsystemSnapshot{});
    assert(unavailable.session_timer);
    assert(!unavailable.lap_timing && !unavailable.logging && !unavailable.g_meter &&
           !unavailable.touch_control && !unavailable.wall_clock);

    const auto degraded = evaluate_features(SubsystemSnapshot{
        SubsystemState::degraded,
        SubsystemState::degraded,
        SubsystemState::unavailable,
        SubsystemState::ready,
        SubsystemState::degraded,
    });
    assert(degraded.session_timer);
    assert(!degraded.lap_timing);
    assert(degraded.logging);
    assert(!degraded.g_meter);
    assert(degraded.touch_control);
    assert(degraded.wall_clock);

    const auto ready = evaluate_features(SubsystemSnapshot{
        SubsystemState::ready,
        SubsystemState::ready,
        SubsystemState::ready,
        SubsystemState::ready,
        SubsystemState::ready,
    });
    assert(ready.session_timer && ready.lap_timing && ready.logging && ready.g_meter &&
           ready.touch_control && ready.wall_clock);
}

void test_file_store_restart()
{
    const auto path = std::filesystem::temp_directory_path() /
                      "track_timer_settings_store_test.bin";
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path.string() + ".tmp", error);
    std::filesystem::remove(path.string() + ".bak", error);

    track_timer::simulator::FileSettingsStore store{path};
    SettingsManager first{store};
    assert(first.load().source == SettingsSource::defaults_missing);
    assert(first.apply(customized_settings(), false) == SettingsApplyResult::applied);

    track_timer::simulator::FileSettingsStore restarted_store{path};
    SettingsManager restarted{restarted_store};
    assert(restarted.load().source == SettingsSource::current);
    assert(settings_equal(restarted.current(), customized_settings()));
    assert(!std::filesystem::exists(path.string() + ".tmp"));
    assert(!std::filesystem::exists(path.string() + ".bak"));

    std::filesystem::remove(path, error);
}

}  // namespace

int main()
{
    test_validation_and_codec();
    test_launch_sensitivity_snapping();
    test_defaults_restart_and_deferred_apply();
    test_migration_corruption_and_storage_errors();
    test_degraded_feature_matrix();
    test_file_store_restart();
    std::cout << "Versioned settings and degraded features passed\n";
    return 0;
}
