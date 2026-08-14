idf_component_register(
    SRCS "component.cpp" "crossing_time.cpp" "crossing_validation.cpp"
         "embedded_runtime.cpp" "engine.cpp" "intersection.cpp" "lap_state_machine.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain esp_timer freertos track
)
