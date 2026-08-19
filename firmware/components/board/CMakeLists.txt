idf_component_register(
    SRCS "component.cpp" "i2c_bus_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain driver esp_driver_i2c
)
