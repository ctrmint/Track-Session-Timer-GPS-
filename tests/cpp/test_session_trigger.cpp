#include "track_timer/ui/session_trigger.hpp"

#include "track_timer/ui/value_picker.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

// The trigger owns pit-exit auto-start rather than sitting beside it. Offering one
// behaviour in two places invites the two disagreeing, which is the rule Mode set.
void the_trigger_owns_pit_exit_auto_start()
{
    settings::DeviceSettings settings{};
    assert(settings.session_trigger == settings::SessionTrigger::manual);
    assert(!settings.pit_exit_auto_start_enabled);

    ui::apply_trigger(settings::SessionTrigger::gps, settings);
    assert(ui::trigger_from_settings(settings) == settings::SessionTrigger::gps);
    assert(settings.pit_exit_auto_start_enabled);

    ui::apply_trigger(settings::SessionTrigger::imu, settings);
    assert(!settings.pit_exit_auto_start_enabled);
    ui::apply_trigger(settings::SessionTrigger::manual, settings);
    assert(!settings.pit_exit_auto_start_enabled);

    // Ending a session on pit entry is a different question from starting one, so it
    // stays where it was.
    settings.pit_entry_auto_stop_enabled = true;
    ui::apply_trigger(settings::SessionTrigger::gps, settings);
    assert(settings.pit_entry_auto_stop_enabled);
}

void the_owned_setting_is_no_longer_offered_as_a_field()
{
    const auto& fields = ui::kPickerFields;
    assert(std::find(fields.begin(), fields.end(),
                     ui::SettingsField::pit_exit_auto_start) == fields.end());
    // Its counterpart is untouched.
    assert(std::find(fields.begin(), fields.end(),
                     ui::SettingsField::pit_entry_auto_stop) != fields.end());
    // And the threshold the IMU trigger reads is still reachable.
    assert(std::find(fields.begin(), fields.end(),
                     ui::SettingsField::launch_sensitivity) != fields.end());
}

// A trigger the driver has selected but that cannot fire is worse than no trigger: the
// device simply sits there. Every case that cannot arm has to say why.
void a_trigger_that_cannot_fire_says_so()
{
    const auto manual =
        ui::trigger_readiness(settings::SessionTrigger::manual, 0, false, false);
    assert(manual.can_arm);

    // LAUNCH at zero means off, so the IMU trigger would wait forever.
    const auto no_threshold =
        ui::trigger_readiness(settings::SessionTrigger::imu, 0, true, false);
    assert(!no_threshold.can_arm);
    assert(std::strlen(no_threshold.detail) > 0);

    // Gravity has to be known before forward acceleration means anything.
    const auto uncalibrated =
        ui::trigger_readiness(settings::SessionTrigger::imu, 500, false, false);
    assert(!uncalibrated.can_arm);
    assert(std::strlen(uncalibrated.detail) > 0);

    const auto ready = ui::trigger_readiness(settings::SessionTrigger::imu, 500, true, false);
    assert(ready.can_arm);
    assert(std::strlen(ready.detail) > 0);  // says what it is waiting for

    // There is no receiver yet, so GPS arms into a wait that cannot end.
    const auto no_gnss =
        ui::trigger_readiness(settings::SessionTrigger::gps, 500, true, false);
    assert(!no_gnss.can_arm);
    assert(std::strlen(no_gnss.detail) > 0);
    assert(ui::trigger_readiness(settings::SessionTrigger::gps, 0, false, true).can_arm);
}

// Forward acceleration only. Braking, cornering and kerbs must not start a session.
void a_launch_is_forward_acceleration()
{
    constexpr std::uint16_t kThreshold = 500;  // 0.50 g

    assert(!ui::launch_detected(0.0F, kThreshold));
    assert(!ui::launch_detected(0.49F, kThreshold));
    assert(ui::launch_detected(0.50F, kThreshold));
    assert(ui::launch_detected(0.90F, kThreshold));

    // Braking is longitudinal too, but the other way, and must never trigger.
    assert(!ui::launch_detected(-0.90F, kThreshold));
    assert(!ui::launch_detected(-3.0F, kThreshold));

    // Zero means off, so nothing fires however hard the car accelerates.
    assert(!ui::launch_detected(4.0F, 0));
}

void every_trigger_is_named_and_labelled()
{
    for (const auto trigger : {settings::SessionTrigger::manual, settings::SessionTrigger::imu,
                               settings::SessionTrigger::gps}) {
        assert(std::strlen(ui::session_trigger_name(trigger)) > 0);
        assert(std::strlen(ui::session_trigger_label(trigger)) > 0);
        assert(std::strlen(ui::session_trigger_summary(trigger)) > 0);
    }
    assert(std::strcmp(ui::session_trigger_label(settings::SessionTrigger::manual),
                       "MANUAL") == 0);
    assert(std::strcmp(ui::session_trigger_label(settings::SessionTrigger::imu), "IMU") == 0);
    assert(std::strcmp(ui::session_trigger_label(settings::SessionTrigger::gps), "GPS") == 0);
}

// The carousel index is cast straight to the enum, so the order has to match.
void the_menu_order_matches_the_enum()
{
    assert(static_cast<std::size_t>(settings::SessionTrigger::manual) == 0);
    assert(static_cast<std::size_t>(settings::SessionTrigger::imu) == 1);
    assert(static_cast<std::size_t>(settings::SessionTrigger::gps) == 2);
    assert(ui::kSessionTriggerCount == 3);
}

}  // namespace

int main()
{
    the_trigger_owns_pit_exit_auto_start();
    the_owned_setting_is_no_longer_offered_as_a_field();
    a_trigger_that_cannot_fire_says_so();
    a_launch_is_forward_acceleration();
    every_trigger_is_named_and_labelled();
    the_menu_order_matches_the_enum();

    std::cout << "Session trigger: ownership of pit-exit auto-start, refusal to arm when "
                 "it cannot fire, and forward-only launch detection passed\n";
    return 0;
}
