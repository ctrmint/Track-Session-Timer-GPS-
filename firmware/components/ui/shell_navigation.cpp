#include "track_timer/ui/shell_navigation.hpp"

namespace track_timer::ui {
namespace {

// Carousels wrap. With three items, clamping would make the far item two swipes away
// for no benefit, and the position indicator keeps the wrap from being disorienting.
template <typename Item>
[[nodiscard]] Item step(const Item current, const std::size_t count,
                        const bool forward) noexcept
{
    const auto index = static_cast<std::size_t>(current);
    const auto next = forward ? (index + 1U) % count : (index + count - 1U) % count;
    return static_cast<Item>(next);
}

[[nodiscard]] NavigationAction action_for(const MenuItem item) noexcept
{
    switch (item) {
    case MenuItem::setup:
        return NavigationAction::open_setup;
    case MenuItem::review:
        return NavigationAction::open_review;
    case MenuItem::diagnostics:
        return NavigationAction::open_diagnostics;
    }
    return NavigationAction::back;
}

}  // namespace

ShellResult ShellNavigation::dispatch(const InputAction action) noexcept
{
    ShellResult result{};

    switch (state_.level) {
    case ShellLevel::ready:
        // Only a deliberate hold opens the menu. A stray tap on the dashboard must not,
        // which is what makes this safe to use in a moving vehicle.
        if (action == InputAction::long_press) {
            if (session_active_) {
                result.outcome = ShellOutcome::refused_session_active;
                break;
            }
            state_.level = ShellLevel::menu;
            state_.menu_item = MenuItem::setup;
            result.outcome = ShellOutcome::menu_opened;
        }
        break;

    case ShellLevel::menu:
        switch (action) {
        case InputAction::swipe_left:
            state_.menu_item = step(state_.menu_item, kMenuItemCount, true);
            result.outcome = ShellOutcome::moved;
            break;
        case InputAction::swipe_right:
            state_.menu_item = step(state_.menu_item, kMenuItemCount, false);
            result.outcome = ShellOutcome::moved;
            break;
        case InputAction::press:
            if (state_.menu_item == MenuItem::setup) {
                state_.level = ShellLevel::section;
                state_.setup_item = SetupItem::device_settings;
            }
            // Review and Diagnostics are single screens, so entering them leaves the
            // shell and hands over to the ordinary destination model.
            result.action = action_for(state_.menu_item);
            result.emits_action = true;
            result.outcome = ShellOutcome::entered;
            break;
        case InputAction::swipe_down:
            state_.level = ShellLevel::ready;
            result.outcome = ShellOutcome::exited;
            break;
        default:
            break;
        }
        break;

    case ShellLevel::section:
        switch (action) {
        case InputAction::swipe_left:
            state_.setup_item = step(state_.setup_item, kSetupItemCount, true);
            result.outcome = ShellOutcome::moved;
            break;
        case InputAction::swipe_right:
            state_.setup_item = step(state_.setup_item, kSetupItemCount, false);
            result.outcome = ShellOutcome::moved;
            break;
        case InputAction::press:
            result.outcome = ShellOutcome::entered;
            break;
        case InputAction::swipe_down:
            state_.level = ShellLevel::menu;
            result.outcome = ShellOutcome::exited;
            break;
        default:
            break;
        }
        break;
    }

    result.state = state_;
    return result;
}

void ShellNavigation::synchronize_session(const bool active) noexcept
{
    session_active_ = active;
    // A session starting while the driver is in a menu returns the screen to the timer;
    // timing authority always wins over configuration.
    if (active && state_.level != ShellLevel::ready) {
        close();
    }
}

void ShellNavigation::close() noexcept
{
    state_.level = ShellLevel::ready;
    state_.menu_item = MenuItem::setup;
    state_.setup_item = SetupItem::device_settings;
}

const ShellState& ShellNavigation::state() const noexcept { return state_; }

bool ShellNavigation::session_active() const noexcept { return session_active_; }

bool ShellNavigation::menu_open() const noexcept
{
    return state_.level != ShellLevel::ready;
}

const char* shell_level_name(const ShellLevel level) noexcept
{
    switch (level) {
    case ShellLevel::ready:
        return "ready";
    case ShellLevel::menu:
        return "menu";
    case ShellLevel::section:
        return "section";
    }
    return "unknown";
}

const char* shell_outcome_name(const ShellOutcome outcome) noexcept
{
    switch (outcome) {
    case ShellOutcome::ignored:
        return "ignored";
    case ShellOutcome::menu_opened:
        return "menu-opened";
    case ShellOutcome::moved:
        return "moved";
    case ShellOutcome::entered:
        return "entered";
    case ShellOutcome::exited:
        return "exited";
    case ShellOutcome::refused_session_active:
        return "refused-session-active";
    }
    return "unknown";
}

const char* menu_item_name(const MenuItem item) noexcept
{
    switch (item) {
    case MenuItem::setup:
        return "setup";
    case MenuItem::review:
        return "review";
    case MenuItem::diagnostics:
        return "diagnostics";
    }
    return "unknown";
}

const char* setup_item_name(const SetupItem item) noexcept
{
    switch (item) {
    case SetupItem::device_settings:
        return "device-settings";
    case SetupItem::track_selection:
        return "track-selection";
    case SetupItem::g_meter:
        return "g-meter";
    }
    return "unknown";
}

}  // namespace track_timer::ui
