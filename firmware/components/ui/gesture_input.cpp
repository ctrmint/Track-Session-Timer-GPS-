#include "track_timer/ui/gesture_input.hpp"

namespace track_timer::ui {
namespace {

struct Binding {
    InputCallback callback{nullptr};
    void* context{nullptr};
    bool gesture_consumed_press{false};
};

// One binding per attached screen. Fixed storage keeps the no-dynamic-allocation rule.
constexpr std::size_t kMaximumBindings = 8;
Binding bindings[kMaximumBindings]{};
std::size_t binding_count = 0;

void handle(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->callback == nullptr) {
        return;
    }

    switch (lv_event_get_code(event)) {
    case LV_EVENT_GESTURE: {
        auto* indev = lv_indev_active();
        if (indev == nullptr) {
            return;
        }
        const auto direction = lv_indev_get_gesture_dir(indev);
        // A recognised swipe must not also register as a tap.
        auto action = InputAction::none;
        if (direction == LV_DIR_LEFT) {
            action = InputAction::swipe_left;
        }
        else if (direction == LV_DIR_RIGHT) {
            action = InputAction::swipe_right;
        }
        else if (direction == LV_DIR_BOTTOM) {
            action = InputAction::swipe_down;
        }
        if (action != InputAction::none) {
            binding->gesture_consumed_press = true;
            lv_indev_wait_release(indev);
            binding->callback(action, binding->context);
        }
        break;
    }
    case LV_EVENT_LONG_PRESSED:
        // A hold is deliberate, so it also cancels the click that would otherwise follow.
        binding->gesture_consumed_press = true;
        if (auto* indev = lv_indev_active(); indev != nullptr) {
            lv_indev_wait_release(indev);
        }
        binding->callback(InputAction::long_press, binding->context);
        break;
    case LV_EVENT_SHORT_CLICKED:
        if (binding->gesture_consumed_press) {
            binding->gesture_consumed_press = false;
            return;
        }
        binding->callback(InputAction::press, binding->context);
        break;
    case LV_EVENT_RELEASED:
        binding->gesture_consumed_press = false;
        break;
    default:
        break;
    }
}

}  // namespace

void attach_gesture_input(lv_obj_t* const target, const InputCallback callback,
                          void* const context) noexcept
{
    if (target == nullptr || callback == nullptr || binding_count >= kMaximumBindings) {
        return;
    }
    auto& binding = bindings[binding_count++];
    binding.callback = callback;
    binding.context = context;
    binding.gesture_consumed_press = false;

    lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(target, handle, LV_EVENT_GESTURE, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_LONG_PRESSED, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_SHORT_CLICKED, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_RELEASED, &binding);
}

}  // namespace track_timer::ui
