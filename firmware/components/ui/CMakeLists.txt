idf_component_register(
    SRCS "component.cpp" "foundation.cpp" "lvgl_visual_system.cpp" "navigation.cpp"
         "presenter.cpp" "ready_screen.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain session board lvgl
)
