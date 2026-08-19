idf_component_register(
    SRCS "qmi8658_esp.cpp" "calibration.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board esp_timer esp_driver_i2c
)
