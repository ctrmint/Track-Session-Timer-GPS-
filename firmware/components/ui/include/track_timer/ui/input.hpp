#pragma once

#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The narrow application-facing input contract asked for by issue #11.
//
// Screens and navigation logic consume these abstract actions and never see LVGL event
// codes, touch coordinates or the FT6336. That keeps the interaction model host-testable
// and means a driver failure cannot reach application state.
enum class InputAction : std::uint8_t {
    none,
    press,        // short tap: select, enter, commit
    long_press,   // hold: open the gated menu from Ready
    swipe_left,   // move to the next item in a carousel
    swipe_right,  // move to the previous item in a carousel
    swipe_down,   // back one level
};

using InputCallback = void (*)(InputAction action, void* context) noexcept;

[[nodiscard]] const char* input_action_name(InputAction action) noexcept;

static_assert(std::is_trivially_copyable_v<InputAction>);

}  // namespace track_timer::ui
