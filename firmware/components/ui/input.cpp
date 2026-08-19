#include "track_timer/ui/input.hpp"

namespace track_timer::ui {

const char* input_action_name(const InputAction action) noexcept
{
    switch (action) {
    case InputAction::none:
        return "none";
    case InputAction::press:
        return "press";
    case InputAction::long_press:
        return "long-press";
    case InputAction::swipe_left:
        return "swipe-left";
    case InputAction::swipe_right:
        return "swipe-right";
    case InputAction::swipe_down:
        return "swipe-down";
    case InputAction::swipe_up:
        return "swipe-up";
    }
    return "unknown";
}

}  // namespace track_timer::ui
