idf_component_register(
    SRCS "panel_esp.cpp" "touch_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board esp_lcd esp_timer driver freertos lvgl
)
