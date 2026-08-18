idf_component_register(
    SRCS "sd_card_esp.cpp"
    INCLUDE_DIRS "include"
    REQUIRES domain board driver fatfs vfs sdmmc esp_driver_sdspi
)
