idf_component_register(
    SRCS "component.cpp" "active_session.cpp" "input.cpp" "shell_navigation.cpp" "gesture_input.cpp" "carousel_screen.cpp" "diagnostics.cpp" "diagnostics_screen.cpp"
         "display_policy.cpp" "foundation.cpp" "gate_capture.cpp" "gate_capture_screen.cpp" "g_meter_screen.cpp" "imu_meter.cpp"
         "lvgl_visual_system.cpp" "navigation.cpp"
         "presenter.cpp" "ready_screen.cpp" "rest_session.cpp" "session_review.cpp" "session_review_screen.cpp"
         "settings_editor.cpp" "settings_screen.cpp"
         "setup_menu_screen.cpp" "track_selection.cpp" "track_selection_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES diagnostics domain session board logger settings track lvgl
)
