#pragma once

// Device composition root for the LVGL screens.
//
// The screen classes and the navigation state machine are host-tested in the ui
// component. This file is the untestable glue that owns one LVGL screen object per
// destination and routes the tested NavigationController's decisions onto them.

namespace track_timer::main_app {

[[nodiscard]] bool start_screen_router() noexcept;

}  // namespace track_timer::main_app
