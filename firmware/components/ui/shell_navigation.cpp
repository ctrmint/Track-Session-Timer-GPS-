#include "track_timer/ui/shell_navigation.hpp"

namespace track_timer::ui {
namespace {

// Carousels wrap. With three or four items, clamping would put the far item several
// swipes away for no benefit, and the position indicator keeps the wrap legible.
[[nodiscard]] std::size_t step(const std::size_t index, const std::size_t count,
                               const bool forward) noexcept
{
    if (count <= 1) {
        return 0;
    }
    return forward ? (index + 1U) % count : (index + count - 1U) % count;
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
    case MenuItem::mode:
    case MenuItem::track:
        break;
    }
    return NavigationAction::back;
}

}  // namespace

ShellNavigation::ShellNavigation(const std::size_t field_count) noexcept
    : field_count_(field_count == 0 ? 1 : field_count)
{
}

std::size_t ShellNavigation::count_for(const ShellLevel level) const noexcept
{
    switch (level) {
    case ShellLevel::menu:
        return kMenuItemCount;
    case ShellLevel::section:
        return section_count_;
    case ShellLevel::field:
        return field_count_;
    case ShellLevel::value:
        return value_count_;
    case ShellLevel::ready:
        break;
    }
    return 0;
}

void ShellNavigation::move(const bool forward, ShellResult& result) noexcept
{
    const auto count = count_for(state_.level);
    switch (state_.level) {
    case ShellLevel::menu:
        state_.menu_index = step(state_.menu_index, count, forward);
        break;
    case ShellLevel::section:
        state_.section_index = step(state_.section_index, count, forward);
        break;
    case ShellLevel::field:
        state_.field_index = step(state_.field_index, count, forward);
        break;
    case ShellLevel::value:
        state_.value_index = step(state_.value_index, count, forward);
        break;
    case ShellLevel::ready:
        return;
    }
    result.outcome = ShellOutcome::moved;
}

void ShellNavigation::enter(ShellResult& result) noexcept
{
    switch (state_.level) {
    case ShellLevel::menu: {
        const auto item = menu_item();
        state_.section_index = 0;
        if (item == MenuItem::mode || item == MenuItem::track ||
            item == MenuItem::setup) {
            state_.level = ShellLevel::section;
            result.outcome = ShellOutcome::entered;
            if (item == MenuItem::setup) {
                result.action = action_for(item);
                result.emits_action = true;
            }
            return;
        }
        // Review and Diagnostics are single screens; hand over to the destination model.
        result.action = action_for(item);
        result.emits_action = true;
        result.outcome = ShellOutcome::entered;
        return;
    }
    case ShellLevel::section:
        if (menu_item() == MenuItem::mode) {
            // Choosing a mode is the whole interaction; the caller persists it and the
            // shell returns the driver to the timer.
            result.outcome = ShellOutcome::mode_selected;
            return;
        }
        if (menu_item() == MenuItem::track) {
            result.outcome = ShellOutcome::track_selected;
            return;
        }
        if (state_.section_index == static_cast<std::size_t>(SetupItem::device_settings)) {
            state_.level = ShellLevel::field;
            state_.field_index = 0;
            result.outcome = ShellOutcome::entered;
            return;
        }
        // Track selection and the G-meter are not routed yet.
        result.outcome = ShellOutcome::ignored;
        return;
    case ShellLevel::field:
        state_.level = ShellLevel::value;
        state_.value_index = 0;
        result.outcome = ShellOutcome::entered;
        return;
    case ShellLevel::value:
        result.outcome = ShellOutcome::value_selected;
        return;
    case ShellLevel::ready:
        return;
    }
}

void ShellNavigation::back(ShellResult& result) noexcept
{
    switch (state_.level) {
    case ShellLevel::value:
        state_.level = ShellLevel::field;
        break;
    case ShellLevel::field:
        state_.level = ShellLevel::section;
        break;
    case ShellLevel::section:
        state_.level = ShellLevel::menu;
        break;
    case ShellLevel::menu:
        state_.level = ShellLevel::ready;
        break;
    case ShellLevel::ready:
        return;
    }
    result.outcome = ShellOutcome::exited;
}

ShellResult ShellNavigation::dispatch(const InputAction action) noexcept
{
    ShellResult result{};

    if (state_.level == ShellLevel::ready) {
        // Only a deliberate hold opens the menu. A stray tap on the dashboard must not,
        // which is what makes this usable in a moving vehicle.
        if (action == InputAction::long_press) {
            if (session_active_) {
                result.outcome = ShellOutcome::refused_session_active;
            }
            else {
                state_.level = ShellLevel::menu;
                state_.menu_index = 0;
                result.outcome = ShellOutcome::menu_opened;
            }
        }
        result.state = state_;
        return result;
    }

    switch (action) {
    case InputAction::swipe_left:
        move(true, result);
        break;
    case InputAction::swipe_right:
        move(false, result);
        break;
    case InputAction::press:
        enter(result);
        break;
    case InputAction::swipe_down:
        back(result);
        break;
    case InputAction::long_press:
        // A hold anywhere inside the menu leaves it, so the driver is never more than
        // one deliberate gesture from the timer.
        close();
        result.outcome = ShellOutcome::exited;
        break;
    case InputAction::none:
        break;
    }

    result.state = state_;
    return result;
}

void ShellNavigation::set_section_count(const std::size_t count) noexcept
{
    section_count_ = count == 0 ? 1 : count;
    if (state_.section_index >= section_count_) {
        state_.section_index = 0;
    }
}

void ShellNavigation::set_value_count(const std::size_t count) noexcept
{
    value_count_ = count == 0 ? 1 : count;
    if (state_.value_index >= value_count_) {
        state_.value_index = 0;
    }
}

void ShellNavigation::select_value(const std::size_t index) noexcept
{
    state_.value_index = index < value_count_ ? index : 0;
}

void ShellNavigation::synchronize_session(const bool active) noexcept
{
    session_active_ = active;
    if (active && state_.level != ShellLevel::ready) {
        close();
    }
}

void ShellNavigation::close() noexcept { state_ = {}; }

const ShellState& ShellNavigation::state() const noexcept { return state_; }

MenuItem ShellNavigation::menu_item() const noexcept
{
    return static_cast<MenuItem>(state_.menu_index);
}

bool ShellNavigation::menu_open() const noexcept
{
    return state_.level != ShellLevel::ready;
}

bool ShellNavigation::session_active() const noexcept { return session_active_; }

const char* shell_level_name(const ShellLevel level) noexcept
{
    switch (level) {
    case ShellLevel::ready:
        return "ready";
    case ShellLevel::menu:
        return "menu";
    case ShellLevel::section:
        return "section";
    case ShellLevel::field:
        return "field";
    case ShellLevel::value:
        return "value";
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
    case ShellOutcome::mode_selected:
        return "mode-selected";
    case ShellOutcome::track_selected:
        return "track-selected";
    case ShellOutcome::value_selected:
        return "value-selected";
    }
    return "unknown";
}

const char* menu_item_name(const MenuItem item) noexcept
{
    switch (item) {
    case MenuItem::mode:
        return "mode";
    case MenuItem::track:
        return "track";
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
