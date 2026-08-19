#include "track_timer/ui/gesture_input.hpp"

namespace track_timer::ui {
namespace {

struct Binding {
    InputCallback callback{nullptr};
    void* context{nullptr};
    // Set when a gesture or hold has already been reported for the touch in progress.
    // Cleared when the next touch begins, NOT on release: LVGL sends LV_EVENT_RELEASED
    // before LV_EVENT_SHORT_CLICKED, so clearing on release let the click through and
    // every swipe also fired a spurious press.
    bool handled_this_touch{false};
};

// One binding per attached screen. Fixed storage keeps the no-dynamic-allocation rule.
constexpr std::size_t kMaximumBindings = 8;
Binding bindings[kMaximumBindings]{};
std::size_t binding_count = 0;
GestureCounters counters{};

void handle(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->callback == nullptr) {
        return;
    }

    switch (lv_event_get_code(event)) {
    case LV_EVENT_GESTURE: {
        ++counters.gesture;
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
            binding->handled_this_touch = true;
            ++counters.dispatched;
            binding->callback(action, binding->context);
        }
        break;
    }
    case LV_EVENT_LONG_PRESSED:
        ++counters.long_pressed;
        ++counters.dispatched;
        // A hold is deliberate, so it also cancels the click that would otherwise follow.
        binding->handled_this_touch = true;
        binding->callback(InputAction::long_press, binding->context);
        break;
    case LV_EVENT_SHORT_CLICKED:
        ++counters.short_clicked;
        if (binding->handled_this_touch) {
            ++counters.suppressed;
            return;
        }
        ++counters.dispatched;
        binding->callback(InputAction::press, binding->context);
        break;
    case LV_EVENT_PRESSED:
        ++counters.pressed;
        binding->handled_this_touch = false;
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
    binding.handled_this_touch = false;

    lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(target, handle, LV_EVENT_PRESSED, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_GESTURE, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_LONG_PRESSED, &binding);
    lv_obj_add_event_cb(target, handle, LV_EVENT_SHORT_CLICKED, &binding);
}

GestureCounters gesture_counters() noexcept { return counters; }

void bubble_gestures_to_parent(lv_obj_t* const child) noexcept
{
    if (child != nullptr) {
        lv_obj_add_flag(child, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
}

}  // namespace track_timer::ui
