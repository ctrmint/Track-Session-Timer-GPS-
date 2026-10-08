idf_component_register(
    SRCS "component.cpp" "active_session.cpp" "input.cpp" "shell_navigation.cpp" "device_mode.cpp" "value_picker.cpp" "gesture_input.cpp" "carousel_screen.cpp" "time_roller.cpp" "time_roller_screen.cpp" "trackday_screen.cpp" "timer_font.cpp" "diagnostics.cpp" "diagnostics_screen.cpp"
         "display_policy.cpp" "foundation.cpp" "gate_capture.cpp" "gate_capture_screen.cpp" "gps_only.cpp" "gps_only_screen.cpp" "g_meter_screen.cpp" "g_radar_screen.cpp" "imu_meter.cpp"
         "lvgl_visual_system.cpp" "navigation.cpp"
         "presenter.cpp" "ready_screen.cpp" "rest_session.cpp" "review_cards.cpp" "session_trigger.cpp" "session_review.cpp" "session_review_screen.cpp"
         "settings_editor.cpp" "settings_screen.cpp"
         "setup_menu_screen.cpp" "track_selection.cpp" "track_selection_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES diagnostics domain session board logger settings track lvgl
    EMBED_FILES "fonts/montserrat_bold_digits.ttf"
)
