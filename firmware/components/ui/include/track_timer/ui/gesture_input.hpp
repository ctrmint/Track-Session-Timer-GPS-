#pragma once

#include "track_timer/ui/input.hpp"

#include <lvgl.h>

#include <cstdint>

namespace track_timer::ui {

// Translates LVGL events on `target` into the abstract InputAction contract. This is the
// only place that knows about LVGL event codes or gesture directions; everything above
// it consumes InputAction and stays host-testable.
//
// LVGL reports a swipe as LV_EVENT_GESTURE and also delivers a click for the same touch,
// so a gesture suppresses the click that follows it. Without that, every swipe would
// also select whatever it swiped onto.
void attach_gesture_input(lv_obj_t* target, InputCallback callback,
                          void* context) noexcept;

// indev_gesture() delivers LV_EVENT_GESTURE to the object under the finger and only
// walks up to its parent while that object has LV_OBJ_FLAG_GESTURE_BUBBLE. Any clickable
// child therefore swallows swipes that begin on it. Call this for such children so a
// swipe starting anywhere still reaches the screen's handler.
void bubble_gestures_to_parent(lv_obj_t* child) noexcept;

enum class DragPhase : std::uint8_t {
    began,
    moved,
    ended,
};

struct DragSample {
    DragPhase phase{DragPhase::began};
    std::int16_t x{0};  // screen coordinates, so a screen can tell which column was touched
    std::int16_t y{0};
    std::int16_t dy{0};  // travel since the previous sample, positive downward
    std::uint32_t elapsed_ms{0};
};

using DragCallback = void (*)(const DragSample& sample, void* context) noexcept;

// Continuous pointer travel, which the InputAction contract deliberately discards when it
// reduces touch to named gestures. A rolling selector needs it: one discrete swipe per
// step would take 59 gestures to cross a column, which is worse than the single press it
// replaces. Everything else should keep using attach_gesture_input.
void attach_drag_input(lv_obj_t* target, DragCallback callback, void* context) noexcept;

// Raw counts of what LVGL actually delivered, so a passive log can distinguish "no
// events arrive" from "events arrive but are suppressed" from "actions are dispatched
// but the screen does not follow".
struct GestureCounters {
    std::uint32_t pressed{0};
    std::uint32_t gesture{0};
    std::uint32_t long_pressed{0};
    std::uint32_t short_clicked{0};
    std::uint32_t dispatched{0};
    std::uint32_t suppressed{0};
};
[[nodiscard]] GestureCounters gesture_counters() noexcept;

}  // namespace track_timer::ui
