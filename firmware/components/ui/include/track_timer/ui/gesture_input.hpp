#pragma once

#include "track_timer/ui/input.hpp"

#include <lvgl.h>

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

}  // namespace track_timer::ui
