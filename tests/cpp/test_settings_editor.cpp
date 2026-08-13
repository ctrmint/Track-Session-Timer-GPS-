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
    constexpr std::array<std::uint16_t, 10> durations{1, 5, 10, 15, 20,
                                                    25, 30, 40, 50, 60};
    while (editor.decrement()) {
    }
    for (const auto expected : durations) {
        assert(editor.draft().session_duration_minutes == expected);
        if (expected != durations.back()) {
            assert(editor.increment());
        }
    }
    assert(!editor.increment());

    auto extended_duration = manager.current();
    extended_duration.session_duration_minutes = 120;
    assert(editor.begin(extended_duration, false));
    assert(!editor.increment());
    assert(editor.decrement());
    assert(editor.draft().session_duration_minutes == 60);
    assert(editor.begin(manager.current(), false));

    select_field(editor, ui::SettingsField::rest_duration);
    while (editor.decrement()) {
    }
    assert(editor.draft().rest_duration_minutes == durations.front());
    while (editor.increment()) {
    }
    assert(editor.draft().rest_duration_minutes == durations.back());

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
    assert(editor.increment());
    editor.request_restore_defaults();
    assert(editor.status() == ui::SettingsEditorStatus::confirm_defaults);
    editor.resolve_restore_defaults(false);
    assert(editor.draft().session_duration_minutes == 25);
    editor.request_restore_defaults();
    editor.resolve_restore_defaults(true);
    assert(editor.draft().session_duration_minutes == 20);
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
