idf_component_register(
    SRCS "component.cpp" "nvs_store_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain nvs_flash
)
