idf_component_register(
    SRCS "component.cpp" "formats.cpp" "async_logger.cpp" "task_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board settings esp_timer freertos
)
