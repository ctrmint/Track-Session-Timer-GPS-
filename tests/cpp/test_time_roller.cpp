#include "track_timer/ui/time_roller.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr std::uint32_t kTick = 50;  // the 20 Hz service tick the device drives this from

ui::TimeRoller rolled(const ui::SettingsField field, const std::uint32_t seconds)
{
    ui::TimeRoller roller{};
    roller.reset(seconds, ui::time_field_spec(field));
    return roller;
}

// One upward flick: several fast drags, then a release.
void flick_up(ui::TimeRoller& roller)
{
    for (int index = 0; index < 5; ++index) {
        roller.drag(-2.0F * ui::kRollerPixelsPerStep, kTick);
    }
    roller.release();
}

// The defect that prompted this: the preset ladder stopped at 3:00 for a field holding
// 59:59, and at 60 minutes for durations validated to 24 hours.
void the_specs_span_the_whole_validated_range()
{
    assert(ui::time_field_spec(ui::SettingsField::average_lap).maximum_seconds == 59 * 60 + 59);
    assert(ui::time_field_spec(ui::SettingsField::session_duration).maximum_seconds ==
           24 * 60 * 60);
    assert(ui::time_field_spec(ui::SettingsField::rest_duration).maximum_seconds ==
           24 * 60 * 60);
    // Session duration cannot be zero, so its floor is a minute.
    assert(ui::time_field_spec(ui::SettingsField::session_duration).minimum_seconds == 60);
    assert(ui::time_field_spec(ui::SettingsField::rest_duration).minimum_seconds == 0);

    assert(ui::is_time_field(ui::SettingsField::average_lap));
    assert(!ui::is_time_field(ui::SettingsField::day_brightness));
}

// A lap over three minutes was unreachable before. Every second of the range now is.
void every_second_of_the_range_is_reachable()
{
    const auto spec = ui::time_field_spec(ui::SettingsField::average_lap);
    for (const std::uint32_t target : {0U, 1U, 59U, 60U, 107U, 500U, 3599U}) {
        ui::TimeRoller roller{};
        roller.reset(target, spec);
        assert(roller.total_seconds() == target);
        assert(roller.minutes() == target / 60U);
        assert(roller.seconds() == target % 60U);
    }
}

// Columns are targeted independently, so a carry would make a fast roll unpredictable and
// would drag minutes along behind a seconds flick.
void columns_wrap_without_carrying()
{
    auto roller = rolled(ui::SettingsField::average_lap, 5 * 60 + 59);
    roller.step(ui::RollerColumn::seconds, 1);
    assert(roller.seconds() == 0);
    assert(roller.minutes() == 5);  // untouched

    roller.step(ui::RollerColumn::seconds, -1);
    assert(roller.seconds() == 59);
    assert(roller.minutes() == 5);

    // Minutes wrap at the field's own ceiling, which differs per field.
    roller = rolled(ui::SettingsField::average_lap, 59 * 60);
    roller.step(ui::RollerColumn::minutes, 1);
    assert(roller.minutes() == 0);

    roller = rolled(ui::SettingsField::session_duration, 1439 * 60);
    roller.step(ui::RollerColumn::minutes, 1);
    assert(roller.minutes() == 0);
}

// The column follows the finger like a physical wheel: pulling down brings lower values up.
void dragging_follows_the_finger()
{
    auto roller = rolled(ui::SettingsField::average_lap, 10 * 60 + 30);
    roller.begin(ui::RollerColumn::seconds);

    roller.drag(3.0F * ui::kRollerPixelsPerStep, kTick);
    assert(roller.seconds() == 27);
    roller.drag(-5.0F * ui::kRollerPixelsPerStep, kTick);
    assert(roller.seconds() == 32);
    assert(roller.minutes() == 10);

    // Sub-step movement accumulates rather than being lost or over-counted.
    auto fine = rolled(ui::SettingsField::average_lap, 60);
    fine.begin(ui::RollerColumn::seconds);
    for (int index = 0; index < 4; ++index) {
        fine.drag(-0.3F * ui::kRollerPixelsPerStep, kTick);
    }
    assert(fine.seconds() == 1);
}

// A discrete swipe still has to work, and 59 of them is why the drag path exists.
void a_swipe_moves_one_step()
{
    auto roller = rolled(ui::SettingsField::average_lap, 90);
    roller.step(ui::RollerColumn::minutes, 1);
    assert(roller.total_seconds() == 150);
    roller.step(ui::RollerColumn::seconds, -1);
    assert(roller.total_seconds() == 149);
}

// The long roll for durations is only usable because a flick carries on after release.
void a_flick_keeps_rolling_and_settles()
{
    auto roller = rolled(ui::SettingsField::session_duration, 20 * 60);
    roller.begin(ui::RollerColumn::minutes);
    const auto before_release = roller.minutes();
    flick_up(roller);
    assert(roller.coasting());

    std::uint32_t elapsed_ms = 0;
    while (roller.advance(kTick)) {
        elapsed_ms += kTick;
        assert(elapsed_ms < 10'000);  // must settle, not spin forever
    }
    assert(!roller.coasting());
    // The coast carried it well beyond where the finger let go.
    assert(roller.minutes() > before_release + 12);
    // And a settled roller stays settled.
    assert(!roller.advance(kTick));
}

// A deliberate slow drag must end exactly where it is released, or a value cannot be set.
void a_slow_release_does_not_flick()
{
    auto roller = rolled(ui::SettingsField::average_lap, 100);
    roller.begin(ui::RollerColumn::seconds);
    for (int index = 0; index < 4; ++index) {
        roller.drag(-0.1F * ui::kRollerPixelsPerStep, 400);
    }
    roller.release();
    assert(!roller.coasting());
    const auto settled = roller.total_seconds();
    assert(!roller.advance(kTick));
    assert(roller.total_seconds() == settled);
}

// Catching a spinning column is how a driver stops one, so a touch must not fight it.
void touching_a_coasting_column_stops_it()
{
    auto roller = rolled(ui::SettingsField::session_duration, 20 * 60);
    roller.begin(ui::RollerColumn::minutes);
    flick_up(roller);
    assert(roller.coasting());
    (void)roller.advance(kTick);

    roller.begin(ui::RollerColumn::minutes);
    assert(!roller.coasting());
    const auto caught = roller.minutes();
    assert(!roller.advance(kTick));
    assert(roller.minutes() == caught);
}

// A roller can sit below a field's floor, so commit clamps rather than writing something
// the settings validator would reject.
void committing_clamps_into_the_validated_range()
{
    ui::TimeRoller roller{};
    roller.reset(0, ui::time_field_spec(ui::SettingsField::session_duration));
    assert(roller.total_seconds() == 0);
    assert(roller.committed_seconds() == 60);

    // Rest has no floor, so zero is a legitimate value there.
    auto rest = rolled(ui::SettingsField::rest_duration, 0);
    assert(rest.committed_seconds() == 0);

    // And a stored value beyond a field's ceiling is pulled back on reset.
    ui::TimeRoller lap{};
    lap.reset(9'999, ui::time_field_spec(ui::SettingsField::average_lap));
    assert(lap.total_seconds() <= 59 * 60 + 59);
}

// Laps remaining cannot be computed without an average, so clearing one clears the other.
void clearing_the_average_lap_resets_the_lower_display()
{
    settings::DeviceSettings draft{};
    draft.average_lap_seconds = 105;
    draft.lower_display = settings::LowerDisplayMode::laps_remaining;

    assert(ui::apply_time_field(ui::SettingsField::average_lap, 0, draft));
    assert(draft.average_lap_seconds == 0);
    assert(draft.lower_display == settings::LowerDisplayMode::elapsed);

    // A real value leaves the dependent setting alone.
    draft.lower_display = settings::LowerDisplayMode::laps_remaining;
    assert(ui::apply_time_field(ui::SettingsField::average_lap, 107, draft));
    assert(draft.lower_display == settings::LowerDisplayMode::laps_remaining);

    // Non-time fields are refused outright, so a stale screen cannot write through here.
    assert(!ui::apply_time_field(ui::SettingsField::day_brightness, 30, draft));
}

void durations_round_trip_through_the_settings()
{
    settings::DeviceSettings draft{};
    assert(ui::apply_time_field(ui::SettingsField::session_duration, 25 * 60 + 30, draft));
    assert(draft.session_duration_seconds == 25 * 60 + 30);
    assert(ui::time_field_seconds(ui::SettingsField::session_duration, draft) == 25 * 60 + 30);

    assert(ui::apply_time_field(ui::SettingsField::rest_duration, 90 * 60, draft));
    assert(ui::time_field_seconds(ui::SettingsField::rest_duration, draft) == 90 * 60);
}

// Whole minutes keep the wording the device has always used, because most values still are.
void formatting_keeps_whole_minutes_readable()
{
    std::array<char, 24> text{};
    ui::format_duration_value(text.data(), text.size(), 20 * 60);
    assert(std::strcmp(text.data(), "20 MIN") == 0);
    ui::format_duration_value(text.data(), text.size(), 20 * 60 + 30);
    assert(std::strcmp(text.data(), "20:30") == 0);
    ui::format_duration_value(text.data(), text.size(), 107);
    assert(std::strcmp(text.data(), "1:47") == 0);
    ui::format_duration_value(text.data(), text.size(), 0);
    assert(std::strcmp(text.data(), "0 MIN") == 0);
    ui::format_duration_value(text.data(), text.size(), 5);
    assert(std::strcmp(text.data(), "0:05") == 0);
}

// LVGL calls a press "long" at 400 ms, which committed a value while the driver was still
// deciding. Saving is the only irreversible thing this screen does, so it asks for more.
void a_save_needs_a_deliberate_hold()
{
    assert(ui::kRollerHoldToSaveMs > 400);

    ui::HoldToSave hold{};
    hold.begin(1'000);
    assert(hold.active());
    assert(!hold.complete(1'000 + 400));                       // LVGL would have fired here
    assert(hold.complete(1'000 + ui::kRollerHoldToSaveMs));
    assert(hold.complete(1'000 + ui::kRollerHoldToSaveMs + 500));
}

// A hold that wanders is a drag that paused, so it must not save what it rolled past.
void a_hold_that_moves_is_not_a_save()
{
    ui::HoldToSave hold{};
    hold.begin(0);
    hold.travel(ui::kRollerHoldTravelLimitPx + 1);
    assert(!hold.active());
    assert(!hold.complete(ui::kRollerHoldToSaveMs * 2));

    // Small movement is tolerated: a finger on a moving car is never perfectly still.
    ui::HoldToSave steady{};
    steady.begin(0);
    steady.travel(2);
    steady.travel(-3);
    assert(steady.active());
    assert(steady.complete(ui::kRollerHoldToSaveMs));
}

// Without feedback a longer hold reads as an unresponsive screen.
void hold_progress_ramps_from_nothing_to_full()
{
    ui::HoldToSave hold{};
    assert(hold.progress(0) == 0.0F);
    hold.begin(500);
    assert(hold.progress(500) == 0.0F);

    const auto half = hold.progress(500 + ui::kRollerHoldToSaveMs / 2);
    assert(half > 0.4F && half < 0.6F);
    assert(hold.progress(500 + ui::kRollerHoldToSaveMs) == 1.0F);
    assert(hold.progress(500 + ui::kRollerHoldToSaveMs * 3) == 1.0F);

    hold.end();
    assert(!hold.active());
    assert(hold.progress(500 + ui::kRollerHoldToSaveMs) == 0.0F);
}

}  // namespace

int main()
{
    the_specs_span_the_whole_validated_range();
    every_second_of_the_range_is_reachable();
    columns_wrap_without_carrying();
    dragging_follows_the_finger();
    a_swipe_moves_one_step();
    a_flick_keeps_rolling_and_settles();
    a_slow_release_does_not_flick();
    touching_a_coasting_column_stops_it();
    committing_clamps_into_the_validated_range();
    clearing_the_average_lap_resets_the_lower_display();
    durations_round_trip_through_the_settings();
    formatting_keeps_whole_minutes_readable();
    a_save_needs_a_deliberate_hold();
    a_hold_that_moves_is_not_a_save();
    hold_progress_ramps_from_nothing_to_full();

    std::cout << "Time roller: full-range columns, wrap without carry, drag and flick, "
                 "clamped commit, and deliberate hold-to-save passed\n";
    return 0;
}
