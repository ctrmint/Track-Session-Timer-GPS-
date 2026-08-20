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
    // When the previous short click landed, so a second one inside the window can be
    // reported as a double tap.
    std::uint32_t last_click_ms{0};
    bool has_last_click{false};
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
        else if (direction == LV_DIR_TOP) {
            action = InputAction::swipe_up;
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
        {
            const auto now_ms = lv_tick_get();
            const auto doubled = binding->has_last_click &&
                                 now_ms - binding->last_click_ms <= kDoubleTapWindowMs;
            // The first tap is still reported. Suppressing it pending a possible second
            // would delay every press by the window, and the screens that act on a double
            // tap ignore single presses anyway.
            binding->has_last_click = !doubled;
            binding->last_click_ms = now_ms;
            ++counters.dispatched;
            binding->callback(doubled ? InputAction::double_tap : InputAction::press,
                              binding->context);
        }
        break;
    case LV_EVENT_PRESSED:
        ++counters.pressed;
        binding->handled_this_touch = false;
        break;
    default:
        break;
    }
}

struct DragBinding {
    DragCallback callback{nullptr};
    void* context{nullptr};
    lv_point_t previous{};
    std::uint32_t previous_ms{0};
    bool active{false};
};

constexpr std::size_t kMaximumDragBindings = 4;
DragBinding drag_bindings[kMaximumDragBindings]{};
std::size_t drag_binding_count = 0;

void handle_drag(lv_event_t* event) noexcept
{
    auto* binding = static_cast<DragBinding*>(lv_event_get_user_data(event));
    if (binding == nullptr || binding->callback == nullptr) {
        return;
    }
    auto* indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t point{};
    lv_indev_get_point(indev, &point);
    const auto now_ms = lv_tick_get();

    DragSample sample{};
    sample.x = static_cast<std::int16_t>(point.x);
    sample.y = static_cast<std::int16_t>(point.y);

    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        binding->previous = point;
        binding->previous_ms = now_ms;
        binding->active = true;
        sample.phase = DragPhase::began;
        break;
    case LV_EVENT_PRESSING:
        if (!binding->active) {
            return;
        }
        sample.phase = DragPhase::moved;
        sample.dy = static_cast<std::int16_t>(point.y - binding->previous.y);
        sample.elapsed_ms = now_ms - binding->previous_ms;
        binding->previous = point;
        binding->previous_ms = now_ms;
        // A sample with no travel says nothing and would only dilute the speed estimate.
        if (sample.dy == 0) {
            return;
        }
        break;
    case LV_EVENT_RELEASED:
        if (!binding->active) {
            return;
        }
        binding->active = false;
        sample.phase = DragPhase::ended;
        break;
    default:
        return;
    }
    binding->callback(sample, binding->context);
}

}  // namespace

void attach_drag_input(lv_obj_t* const target, const DragCallback callback,
                       void* const context) noexcept
{
    if (target == nullptr || callback == nullptr ||
        drag_binding_count >= kMaximumDragBindings) {
        return;
    }
    auto& binding = drag_bindings[drag_binding_count++];
    binding.callback = callback;
    binding.context = context;
    binding.active = false;

    lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(target, handle_drag, LV_EVENT_PRESSED, &binding);
    lv_obj_add_event_cb(target, handle_drag, LV_EVENT_PRESSING, &binding);
    lv_obj_add_event_cb(target, handle_drag, LV_EVENT_RELEASED, &binding);
}

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
