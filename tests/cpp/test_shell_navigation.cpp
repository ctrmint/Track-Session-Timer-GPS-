#include "track_timer/ui/shell_navigation.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer::ui;

constexpr std::size_t kFields = 12;

ShellNavigation opened()
{
    ShellNavigation shell{kFields};
    (void)shell.dispatch(InputAction::long_press);
    return shell;
}

// The whole point of the hold gate: ordinary taps and swipes on the dashboard must not
// open the menu, or a driver lands in configuration mid-session.
void only_a_hold_opens_the_menu()
{
    ShellNavigation shell{kFields};
    for (const auto action : {InputAction::press, InputAction::swipe_left,
                              InputAction::swipe_right, InputAction::swipe_down,
                              InputAction::none}) {
        const auto result = shell.dispatch(action);
        assert(result.outcome == ShellOutcome::ignored);
        assert(result.state.level == ShellLevel::ready);
    }
    const auto open = shell.dispatch(InputAction::long_press);
    assert(open.outcome == ShellOutcome::menu_opened);
    assert(open.state.level == ShellLevel::menu);
    assert(shell.menu_item() == MenuItem::mode);
}

// Hops to a menu item by name. Counting swipes meant that inserting an item silently
// pointed these tests at a different one.
void to_menu(ShellNavigation& shell, const MenuItem item)
{
    for (std::size_t guard = 0; guard <= kMenuItemCount; ++guard) {
        if (shell.menu_item() == item) {
            return;
        }
        (void)shell.dispatch(InputAction::swipe_left);
    }
    assert(false && "menu item not reachable");
}

// Track and Trigger sit between Mode and Setup: at a circuit both are set before going
// out, so neither should be buried under Setup with the rarely-touched options.
void the_menu_offers_mode_then_track_and_wraps()
{
    auto shell = opened();
    assert(shell.menu_item() == MenuItem::mode);
    for (const auto expected : {MenuItem::track, MenuItem::trigger, MenuItem::setup,
                                MenuItem::review, MenuItem::diagnostics, MenuItem::mode}) {
        (void)shell.dispatch(InputAction::swipe_left);
        assert(shell.menu_item() == expected);
    }
    (void)shell.dispatch(InputAction::swipe_right);
    assert(shell.menu_item() == MenuItem::diagnostics);
}

// The list length comes from the card, so the shell must be told and must wrap over it.
void the_track_list_is_sized_by_the_caller()
{
    auto shell = opened();
    (void)shell.dispatch(InputAction::swipe_left);      // TRACK
    assert(shell.menu_item() == MenuItem::track);
    auto result = shell.dispatch(InputAction::press);
    assert(result.outcome == ShellOutcome::entered);
    assert(result.state.level == ShellLevel::section);
    assert(!result.emits_action);                       // track has no destination screen

    shell.set_section_count(25);
    for (std::size_t index = 1; index < 25; ++index) {
        assert(shell.dispatch(InputAction::swipe_left).state.section_index == index);
    }
    // Wraps at the end rather than sticking on the last circuit.
    assert(shell.dispatch(InputAction::swipe_left).state.section_index == 0);
    assert(shell.dispatch(InputAction::swipe_right).state.section_index == 24);

    result = shell.dispatch(InputAction::press);
    assert(result.outcome == ShellOutcome::track_selected);
    assert(result.state.section_index == 24);
}

// A shorter list after a card change must not leave the cursor past the end.
void a_shorter_track_list_resets_an_out_of_range_index()
{
    auto shell = opened();
    (void)shell.dispatch(InputAction::swipe_left);
    (void)shell.dispatch(InputAction::press);
    shell.set_section_count(25);
    for (int i = 0; i < 20; ++i) {
        (void)shell.dispatch(InputAction::swipe_left);
    }
    assert(shell.state().section_index == 20);
    shell.set_section_count(3);
    assert(shell.state().section_index == 0);
}

// Choosing a mode is the entire interaction: enter Mode, swipe to it, press.
void a_mode_is_three_gestures_from_the_dashboard()
{
    auto shell = opened();                      // 1: hold
    auto result = shell.dispatch(InputAction::press);   // 2: enter Mode
    assert(result.outcome == ShellOutcome::entered);
    assert(result.state.level == ShellLevel::section);
    assert(!result.emits_action);               // Mode has no destination screen

    result = shell.dispatch(InputAction::swipe_left);   // 3: to Race
    assert(result.state.section_index == 1);
    result = shell.dispatch(InputAction::press);        // 4: select
    assert(result.outcome == ShellOutcome::mode_selected);
    assert(result.state.section_index == 1);
}

void review_and_diagnostics_hand_over_to_the_destination_model()
{
    auto shell = opened();
    // Swiping to the item by name rather than by a count, so inserting a menu item does
    // not silently point this test at a different one.
    to_menu(shell, MenuItem::review);
    auto result = shell.dispatch(InputAction::press);
    assert(result.emits_action && result.action == NavigationAction::open_review);
    assert(result.state.level == ShellLevel::menu);

    (void)shell.dispatch(InputAction::swipe_left);
    result = shell.dispatch(InputAction::press);
    assert(result.emits_action && result.action == NavigationAction::open_diagnostics);
}

// Setup descends: section -> field -> value, one press per level.
void a_setting_value_is_reachable_by_descending_three_levels()
{
    auto shell = opened();
    to_menu(shell, MenuItem::setup);
    auto result = shell.dispatch(InputAction::press);
    assert(result.state.level == ShellLevel::section);
    assert(result.emits_action && result.action == NavigationAction::open_setup);

    result = shell.dispatch(InputAction::press);              // DEVICE SETTINGS
    assert(result.state.level == ShellLevel::field);

    shell.set_value_count(10);
    result = shell.dispatch(InputAction::press);              // open the field
    assert(result.state.level == ShellLevel::value);

    result = shell.dispatch(InputAction::swipe_left);
    assert(result.state.value_index == 1);
    result = shell.dispatch(InputAction::press);              // choose the value
    assert(result.outcome == ShellOutcome::value_selected);
    assert(result.state.value_index == 1);
}

void every_level_climbs_back_out_one_at_a_time()
{
    auto shell = opened();
    to_menu(shell, MenuItem::setup);
    (void)shell.dispatch(InputAction::press);
    (void)shell.dispatch(InputAction::press);
    shell.set_value_count(4);
    (void)shell.dispatch(InputAction::press);
    assert(shell.state().level == ShellLevel::value);

    for (const auto expected : {ShellLevel::field, ShellLevel::section, ShellLevel::menu,
                                ShellLevel::ready}) {
        const auto result = shell.dispatch(InputAction::swipe_down);
        assert(result.outcome == ShellOutcome::exited);
        assert(result.state.level == expected);
    }
    // Already out: swiping down again does nothing rather than underflowing.
    assert(shell.dispatch(InputAction::swipe_down).outcome == ShellOutcome::ignored);
}

// However deep the driver is, one hold returns them to the timer.
void a_hold_anywhere_inside_the_menu_returns_to_the_timer()
{
    auto shell = opened();
    to_menu(shell, MenuItem::setup);
    (void)shell.dispatch(InputAction::press);
    (void)shell.dispatch(InputAction::press);
    assert(shell.state().level == ShellLevel::field);

    const auto result = shell.dispatch(InputAction::long_press);
    assert(result.outcome == ShellOutcome::exited);
    assert(result.state.level == ShellLevel::ready);
    assert(!shell.menu_open());
}

void a_live_session_blocks_and_closes_the_menu()
{
    ShellNavigation shell{kFields};
    shell.synchronize_session(true);
    assert(shell.dispatch(InputAction::long_press).outcome ==
           ShellOutcome::refused_session_active);
    assert(!shell.menu_open());

    shell.synchronize_session(false);
    (void)shell.dispatch(InputAction::long_press);
    (void)shell.dispatch(InputAction::press);
    assert(shell.state().level == ShellLevel::section);

    shell.synchronize_session(true);
    assert(shell.state().level == ShellLevel::ready);
}

// A field with fewer values than the last one must not leave a stale index behind.
void changing_the_value_count_resets_an_out_of_range_index()
{
    auto shell = opened();
    to_menu(shell, MenuItem::setup);
    (void)shell.dispatch(InputAction::press);
    (void)shell.dispatch(InputAction::press);
    shell.set_value_count(10);
    (void)shell.dispatch(InputAction::press);
    for (int i = 0; i < 8; ++i) {
        (void)shell.dispatch(InputAction::swipe_left);
    }
    assert(shell.state().value_index == 8);
    shell.set_value_count(2);
    assert(shell.state().value_index == 0);
}

void names_are_stable_for_logging_and_replay()
{
    assert(std::strcmp(input_action_name(InputAction::long_press), "long-press") == 0);
    assert(std::strcmp(shell_level_name(ShellLevel::value), "value") == 0);
    assert(std::strcmp(shell_outcome_name(ShellOutcome::mode_selected), "mode-selected") == 0);
    assert(std::strcmp(menu_item_name(MenuItem::mode), "mode") == 0);
    assert(std::strcmp(setup_item_name(SetupItem::track_selection), "track-selection") == 0);
}

// A menu that always opens on its first item cannot tell the driver what is set, only let
// them change it. The value level already did this; the section level did not.
void a_section_opens_on_the_item_in_force()
{
    auto shell = opened();
    to_menu(shell, MenuItem::trigger);
    (void)shell.dispatch(InputAction::press);
    assert(shell.state().level == ShellLevel::section);
    shell.set_section_count(3);

    // Entering resets to the first item, which is what the caller then corrects.
    assert(shell.state().section_index == 0);
    shell.select_section(2);
    assert(shell.state().section_index == 2);

    // And it still moves from there rather than snapping back.
    (void)shell.dispatch(InputAction::swipe_left);
    assert(shell.state().section_index == 0);  // wrapped past the end of three
    (void)shell.dispatch(InputAction::swipe_right);
    assert(shell.state().section_index == 2);
}

// A stored track that is no longer on the card must not leave an index pointing past the
// end of a shorter catalog.
void an_out_of_range_section_falls_back_to_the_first()
{
    auto shell = opened();
    to_menu(shell, MenuItem::track);
    (void)shell.dispatch(InputAction::press);
    shell.set_section_count(3);
    shell.select_section(7);
    assert(shell.state().section_index == 0);

    // Shrinking the catalog under a valid selection has to be safe too.
    shell.select_section(2);
    shell.set_section_count(1);
    assert(shell.state().section_index == 0);
}

}  // namespace

int main()
{
    only_a_hold_opens_the_menu();
    the_menu_offers_mode_then_track_and_wraps();
    the_track_list_is_sized_by_the_caller();
    a_shorter_track_list_resets_an_out_of_range_index();
    a_mode_is_three_gestures_from_the_dashboard();
    review_and_diagnostics_hand_over_to_the_destination_model();
    a_setting_value_is_reachable_by_descending_three_levels();
    every_level_climbs_back_out_one_at_a_time();
    a_hold_anywhere_inside_the_menu_returns_to_the_timer();
    a_live_session_blocks_and_closes_the_menu();
    changing_the_value_count_resets_an_out_of_range_index();
    names_are_stable_for_logging_and_replay();

    a_section_opens_on_the_item_in_force();
    an_out_of_range_section_falls_back_to_the_first();

    std::cout << "Hold gate, Mode selection, descending editor levels, and session lock "
                 "passed\n";
    return 0;
}
