idf_component_register(
    SRCS "app_main.cpp" "screen_router.cpp"
    INCLUDE_DIRS "."
    REQUIRES domain board gnss timing track session logger ui diagnostics display
             esp_app_format esp_system esp_timer heap lvgl storage catalog timing imu session
)

# One-shot microSD format, opt-in through the build environment only. Never enabled by
# a normal build: TRACK_TIMER_FORMAT_SD must be set explicitly in the environment.
if(DEFINED ENV{TRACK_TIMER_FORMAT_SD})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TRACK_TIMER_FORMAT_SD_ONCE=1)
    message(WARNING "microSD one-shot format is ENABLED for this build")
endif()
