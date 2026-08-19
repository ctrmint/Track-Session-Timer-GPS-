#include "track_timer/settings/settings.hpp"
#include "track_timer/ui/settings_editor.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <iostream>

namespace {

class MemoryStore final : public track_timer::settings::SettingsStore {
  public:
    track_timer::settings::StoreReadResult read(
        track_timer::settings::SettingsBlob& output) noexcept override
    {
        if (!has_value) {
            return track_timer::settings::StoreReadResult::missing;
        }
        output = blob;
        return track_timer::settings::StoreReadResult::found;
    }

    bool write_atomic(const track_timer::settings::SettingsBlob& value) noexcept override
    {
        if (fail_writes) {
            return false;
        }
        blob = value;
        has_value = true;
        return true;
    }

    track_timer::settings::SettingsBlob blob{};
    bool has_value{false};
    bool fail_writes{false};
};

void select_field(track_timer::ui::SettingsEditor& editor,
                  const track_timer::ui::SettingsField field)
{
    for (std::size_t count = 0;
         editor.field() != field && count < track_timer::ui::kSettingsFieldCount; ++count) {
        editor.next_field();
    }
    assert(editor.field() == field);
}

}  // namespace

int main()
{
    using namespace track_timer;

    MemoryStore store{};
    settings::SettingsManager manager{store};
    (void)manager.load();
    ui::SettingsEditor editor{};

    assert(!editor.begin(manager.current(), true));
    assert(editor.status() == ui::SettingsEditorStatus::locked_active);
    assert(!editor.increment());

    assert(editor.begin(manager.current(), false));
    // Seconds since v5. The stepping editor's ladder is unchanged in what it means, only
    // in the unit it is expressed in.
    constexpr std::array<std::uint32_t, 10> durations{60,   300,  600,  900,  1200,
                                                     1500, 1800, 2400, 3000, 3600};
    while (editor.decrement()) {
    }
    for (const auto expected : durations) {
        assert(editor.draft().session_duration_seconds == expected);
        if (expected != durations.back()) {
            assert(editor.increment());
        }
    }
    assert(!editor.increment());

    auto extended_duration = manager.current();
    extended_duration.session_duration_seconds = 120 * 60;
    assert(editor.begin(extended_duration, false));
    assert(!editor.increment());
    assert(editor.decrement());
    assert(editor.draft().session_duration_seconds == 3600);
    assert(editor.begin(manager.current(), false));

    select_field(editor, ui::SettingsField::rest_duration);
    while (editor.decrement()) {
    }
    assert(editor.draft().rest_duration_seconds == durations.front());
    while (editor.increment()) {
    }
    assert(editor.draft().rest_duration_seconds == durations.back());

    select_field(editor, ui::SettingsField::launch_sensitivity);
    constexpr std::array<std::uint16_t, 10> launch_values{0, 500, 1'000, 1'250, 1'500,
                                                         1'750, 2'000, 2'500, 3'500, 4'000};
    for (const auto expected : launch_values) {
        assert(editor.draft().launch_sensitivity_milli_g == expected);
        if (expected != launch_values.back()) {
            assert(editor.increment());
        }
    }
    assert(!editor.increment());

    select_field(editor, ui::SettingsField::day_brightness);
    while (editor.decrement()) {
    }
    for (const auto expected : {25U, 50U, 75U, 100U}) {
        assert(editor.draft().day_brightness_percent == expected);
        if (expected != 100U) {
            assert(editor.increment());
        }
    }

    select_field(editor, ui::SettingsField::night_brightness);
    while (editor.decrement()) {
    }
    assert(editor.draft().night_brightness_percent == 25);
    while (editor.increment()) {
    }
    assert(editor.draft().night_brightness_percent == 100);

    select_field(editor, ui::SettingsField::operating_mode);
    assert(editor.increment());
    assert(editor.draft().operating_mode == settings::OperatingMode::g_meter);
    assert(editor.increment());
    assert(editor.draft().operating_mode == settings::OperatingMode::timer);

    select_field(editor, ui::SettingsField::trackday_mode);
    assert(editor.increment());
    assert(editor.draft().trackday_mode_enabled);
    assert(editor.decrement());
    assert(!editor.draft().trackday_mode_enabled);

    select_field(editor, ui::SettingsField::lap_boundary);
    assert(editor.draft().lap_boundary == settings::LapBoundaryMode::finish);
    assert(editor.increment());
    assert(editor.draft().lap_boundary == settings::LapBoundaryMode::start);
    assert(editor.decrement());
    assert(editor.draft().lap_boundary == settings::LapBoundaryMode::finish);

    select_field(editor, ui::SettingsField::pit_exit_auto_start);
    assert(editor.increment());
    assert(editor.draft().pit_exit_auto_start_enabled);
    assert(editor.decrement());
    assert(!editor.draft().pit_exit_auto_start_enabled);

    select_field(editor, ui::SettingsField::pit_entry_auto_stop);
    assert(editor.increment());
    assert(editor.draft().pit_entry_auto_stop_enabled);
    assert(editor.decrement());
    assert(!editor.draft().pit_entry_auto_stop_enabled);

    select_field(editor, ui::SettingsField::orientation);
    for (std::uint8_t expected = 1; expected <= 4; ++expected) {
        assert(editor.increment());
        assert(static_cast<std::uint8_t>(editor.draft().orientation) == expected);
    }
    assert(editor.increment());
    assert(editor.draft().orientation == settings::OrientationMode::fixed_0);

    select_field(editor, ui::SettingsField::auto_dim);
    assert(editor.increment());
    assert(editor.draft().auto_dim_enabled);
    assert(editor.decrement());
    assert(!editor.draft().auto_dim_enabled);

    select_field(editor, ui::SettingsField::lower_display);
    assert(!editor.increment());
    assert(editor.status() == ui::SettingsEditorStatus::invalid);
    assert(editor.draft().lower_display == settings::LowerDisplayMode::elapsed);

    select_field(editor, ui::SettingsField::average_lap);
    assert(!editor.decrement());
    for (std::uint16_t second = 0; second < 59 * 60 + 59; ++second) {
        assert(editor.increment());
    }
    assert(editor.draft().average_lap_seconds == 59 * 60 + 59);
    assert(!editor.increment());
    select_field(editor, ui::SettingsField::lower_display);
    assert(editor.increment());
    assert(editor.draft().lower_display == settings::LowerDisplayMode::laps_remaining);
    select_field(editor, ui::SettingsField::average_lap);
    while (editor.decrement()) {
    }
    assert(editor.draft().average_lap_seconds == 0);
    assert(editor.draft().lower_display == settings::LowerDisplayMode::elapsed);

    auto with_track = manager.current();
    std::strcpy(with_track.selected_track_id.data(), "silverstone-gp");
    assert(editor.begin(with_track, false));
    select_field(editor, ui::SettingsField::trackday_mode);
    assert(editor.increment());
    assert(editor.draft().trackday_mode_enabled);
    editor.request_restore_defaults();
    assert(editor.status() == ui::SettingsEditorStatus::confirm_defaults);
    editor.resolve_restore_defaults(false);
    assert(editor.draft().trackday_mode_enabled);
    editor.request_restore_defaults();
    editor.resolve_restore_defaults(true);
    assert(editor.draft().session_duration_seconds == 20 * 60);
    assert(!editor.draft().trackday_mode_enabled);
    assert(std::strcmp(editor.draft().selected_track_id.data(), "silverstone-gp") == 0);

    assert(editor.save(manager, false) == settings::SettingsApplyResult::applied);
    assert(editor.status() == ui::SettingsEditorStatus::applied);
    assert(std::strcmp(manager.current().selected_track_id.data(), "silverstone-gp") == 0);

    assert(editor.begin(manager.current(), false));
    assert(editor.increment());
    assert(editor.save(manager, true) == settings::SettingsApplyResult::deferred);
    assert(editor.status() == ui::SettingsEditorStatus::deferred);
    assert(manager.has_pending_change());
    assert(manager.apply_deferred(false) == settings::SettingsApplyResult::applied);

    assert(editor.begin(manager.current(), false));
    assert(editor.increment());
    store.fail_writes = true;
    assert(editor.save(manager, false) == settings::SettingsApplyResult::storage_error);
    assert(editor.status() == ui::SettingsEditorStatus::storage_error);

    editor.cancel();
    assert(editor.status() == ui::SettingsEditorStatus::cancelled);
    assert(!editor.changed());

    std::cout << "Settings editor options, boundaries, defaults, and safe saves passed\n";
    return 0;
}
