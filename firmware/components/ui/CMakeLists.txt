idf_component_register(
    SRCS "component.cpp" "foundation.cpp" "lvgl_visual_system.cpp" "presenter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain session board lvgl
)
