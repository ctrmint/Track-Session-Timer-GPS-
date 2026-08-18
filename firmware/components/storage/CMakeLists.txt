idf_component_register(
    SRCS "sd_card_esp.cpp" "sd_track_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board track driver fatfs vfs sdmmc esp_driver_sdspi
)
