#include "track_timer/ui/shell_navigation.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer::ui;

// The whole point of the hold gate: ordinary taps and swipes on the dashboard must not
// open the menu, or a driver will land in configuration mid-session.
void only_a_hold_opens_the_menu()
{
    ShellNavigation shell;
    for (const auto action : {InputAction::press, InputAction::swipe_left,
                              InputAction::swipe_right, InputAction::swipe_down,
                              InputAction::none}) {
        const auto result = shell.dispatch(action);
        assert(result.outcome == ShellOutcome::ignored);
        assert(result.state.level == ShellLevel::ready);
        assert(!shell.menu_open());
    }

    const auto opened = shell.dispatch(InputAction::long_press);
    assert(opened.outcome == ShellOutcome::menu_opened);
    assert(opened.state.level == ShellLevel::menu);
    assert(opened.state.menu_item == MenuItem::setup);
    assert(shell.menu_open());
}

void the_menu_carousel_wraps_in_both_directions()
{
    ShellNavigation shell;
    (void)shell.dispatch(InputAction::long_press);

    assert(shell.dispatch(InputAction::swipe_left).state.menu_item == MenuItem::review);
    assert(shell.dispatch(InputAction::swipe_left).state.menu_item ==
           MenuItem::diagnostics);
    // Wrapping means the far item is never more than one swipe away.
    assert(shell.dispatch(InputAction::swipe_left).state.menu_item == MenuItem::setup);
    assert(shell.dispatch(InputAction::swipe_right).state.menu_item ==
           MenuItem::diagnostics);
    assert(shell.dispatch(InputAction::swipe_right).state.menu_item == MenuItem::review);
}

// Entering Setup descends into its own carousel; Review and Diagnostics are single
// screens and hand straight over to the destination model.
void entering_setup_descends_but_review_and_diagnostics_hand_over()
{
    ShellNavigation shell;
    (void)shell.dispatch(InputAction::long_press);
    const auto setup = shell.dispatch(InputAction::press);
    assert(setup.outcome == ShellOutcome::entered);
    assert(setup.emits_action && setup.action == NavigationAction::open_setup);
    assert(setup.state.level == ShellLevel::section);
    assert(setup.state.setup_item == SetupItem::device_settings);

    ShellNavigation other;
    (void)other.dispatch(InputAction::long_press);
    (void)other.dispatch(InputAction::swipe_left);
    const auto review = other.dispatch(InputAction::press);
    assert(review.emits_action && review.action == NavigationAction::open_review);
    assert(review.state.level == ShellLevel::menu);

    (void)other.dispatch(InputAction::swipe_left);
    const auto diagnostics = other.dispatch(InputAction::press);
    assert(diagnostics.emits_action &&
           diagnostics.action == NavigationAction::open_diagnostics);
}

void the_setup_carousel_wraps_and_swipe_down_climbs_out()
{
    ShellNavigation shell;
    (void)shell.dispatch(InputAction::long_press);
    (void)shell.dispatch(InputAction::press);

    assert(shell.dispatch(InputAction::swipe_left).state.setup_item ==
           SetupItem::track_selection);
    assert(shell.dispatch(InputAction::swipe_left).state.setup_item ==
           SetupItem::g_meter);
    assert(shell.dispatch(InputAction::swipe_left).state.setup_item ==
           SetupItem::device_settings);

    const auto up = shell.dispatch(InputAction::swipe_down);
    assert(up.outcome == ShellOutcome::exited);
    assert(up.state.level == ShellLevel::menu);

    const auto out = shell.dispatch(InputAction::swipe_down);
    assert(out.outcome == ShellOutcome::exited);
    assert(out.state.level == ShellLevel::ready);
    assert(!shell.menu_open());

    // Already at the top: swiping down again does nothing rather than underflowing.
    const auto again = shell.dispatch(InputAction::swipe_down);
    assert(again.outcome == ShellOutcome::ignored);
    assert(again.state.level == ShellLevel::ready);
}

// Timing authority beats configuration, matching the existing settings lock.
void a_live_session_blocks_and_closes_the_menu()
{
    ShellNavigation shell;
    shell.synchronize_session(true);
    const auto refused = shell.dispatch(InputAction::long_press);
    assert(refused.outcome == ShellOutcome::refused_session_active);
    assert(refused.state.level == ShellLevel::ready);
    assert(!shell.menu_open());

    shell.synchronize_session(false);
    assert(shell.dispatch(InputAction::long_press).outcome == ShellOutcome::menu_opened);
    (void)shell.dispatch(InputAction::press);
    assert(shell.state().level == ShellLevel::section);

    // A session starting while the driver is deep in a menu returns them to the timer.
    shell.synchronize_session(true);
    assert(shell.state().level == ShellLevel::ready);
    assert(!shell.menu_open());
}

// Every level must be escapable without a swipe, because a gloved hand may not register
// one on a capacitive panel. Reopening always starts from a known position.
void reopening_the_menu_is_deterministic()
{
    ShellNavigation shell;
    (void)shell.dispatch(InputAction::long_press);
    (void)shell.dispatch(InputAction::swipe_left);
    (void)shell.dispatch(InputAction::swipe_left);
    shell.close();
    assert(shell.state().level == ShellLevel::ready);

    const auto reopened = shell.dispatch(InputAction::long_press);
    assert(reopened.state.menu_item == MenuItem::setup);
    assert(reopened.state.setup_item == SetupItem::device_settings);
}

void names_are_stable_for_logging_and_replay()
{
    assert(std::strcmp(input_action_name(InputAction::long_press), "long-press") == 0);
    assert(std::strcmp(shell_level_name(ShellLevel::section), "section") == 0);
    assert(std::strcmp(shell_outcome_name(ShellOutcome::refused_session_active),
                       "refused-session-active") == 0);
    assert(std::strcmp(menu_item_name(MenuItem::diagnostics), "diagnostics") == 0);
    assert(std::strcmp(setup_item_name(SetupItem::track_selection), "track-selection") ==
           0);
}

}  // namespace

int main()
{
    only_a_hold_opens_the_menu();
    the_menu_carousel_wraps_in_both_directions();
    entering_setup_descends_but_review_and_diagnostics_hand_over();
    the_setup_carousel_wraps_and_swipe_down_climbs_out();
    a_live_session_blocks_and_closes_the_menu();
    reopening_the_menu_is_deterministic();
    names_are_stable_for_logging_and_replay();

    std::cout << "Hold-gated menu, wrapping carousels, session lock, and deterministic "
                 "reopen passed\n";
    return 0;
}
