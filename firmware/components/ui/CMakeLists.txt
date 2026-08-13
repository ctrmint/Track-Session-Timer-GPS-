idf_component_register(
    SRCS "component.cpp" "foundation.cpp" "lvgl_visual_system.cpp" "navigation.cpp"
         "presenter.cpp" "ready_screen.cpp" "settings_editor.cpp" "settings_screen.cpp"
         "setup_menu_screen.cpp" "track_selection.cpp" "track_selection_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain session board settings track lvgl
)
