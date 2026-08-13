cmake_minimum_required(VERSION 3.22)

project(track_timer_device_simulator LANGUAGES C CXX)

include(FetchContent)
include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
    URL_HASH SHA256=34a955cdf3a2d005507b704e87357af669a114523b6d3f77b5344fdc68717bc6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

add_library(
    track_timer_simulator_model
    ../firmware/components/ui/presenter.cpp
    ../firmware/components/settings/component.cpp
    src/device_backends.cpp
    src/file_settings_store.cpp
    src/fixed_cell_text.cpp
    src/scenario.cpp
)
target_include_directories(
    track_timer_simulator_model
    PUBLIC
        include
        ../firmware/components/board/include
        ../firmware/components/domain/include
        ../firmware/components/settings/include
        ../firmware/components/ui/include
)
target_compile_features(track_timer_simulator_model PUBLIC cxx_std_17)

add_executable(
    track_timer_simulator
    src/device_screen.cpp
    src/fixed_cell_label.cpp
    src/main.cpp
)
target_include_directories(track_timer_simulator PRIVATE src)
target_link_libraries(
    track_timer_simulator
    PRIVATE
        track_timer_simulator_model
        lvgl::lvgl
        SDL2::SDL2
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
        track_timer_simulator_model
        PRIVATE -Wall -Wextra -Werror -pedantic
    )
    target_compile_options(
        track_timer_simulator
        PRIVATE -Wall -Wextra -Werror -pedantic
    )
endif()

if(BUILD_TESTING)
    add_executable(
        track_timer_settings_test
        ../tests/cpp/test_settings.cpp
    )
    target_link_libraries(
        track_timer_settings_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_settings_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-versioned-settings
        COMMAND track_timer_settings_test
    )
    set_tests_properties(
        simulator-versioned-settings
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Versioned settings and degraded features passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_fixed_cell_label_test
        ../tests/cpp/test_fixed_cell_label.cpp
        src/fixed_cell_label.cpp
    )
    target_include_directories(track_timer_fixed_cell_label_test PRIVATE src)
    target_link_libraries(
        track_timer_fixed_cell_label_test
        PRIVATE
            track_timer_simulator_model
            lvgl::lvgl
            SDL2::SDL2
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_fixed_cell_label_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-fixed-cell-timer-layout
        COMMAND track_timer_fixed_cell_label_test
    )
    set_tests_properties(
        simulator-fixed-cell-timer-layout
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "Fixed-cell LVGL timer positions passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_device_screen_test
        ../tests/cpp/test_device_screen.cpp
        src/device_screen.cpp
        src/fixed_cell_label.cpp
    )
    target_include_directories(track_timer_device_screen_test PRIVATE src)
    target_link_libraries(
        track_timer_device_screen_test
        PRIVATE
            track_timer_simulator_model
            lvgl::lvgl
            SDL2::SDL2
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_device_screen_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-gnss-indicator
        COMMAND track_timer_device_screen_test
    )
    set_tests_properties(
        simulator-gnss-indicator
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "GPS indicator symbol, colours, and position passed"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-active-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --frames 4
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/active.ppm"
    )
    set_tests_properties(
        simulator-active-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=active.*resolution=600x450"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-gnss-loss-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario gnss-loss
            --frames 4
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/gnss-loss.ppm"
    )
    set_tests_properties(
        simulator-gnss-loss-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=gnss-loss.*gnss=GPS STALE.*gnss-mode=loss"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-gnss-recovery-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario gnss-loss
            --frames 125
    )
    set_tests_properties(
        simulator-gnss-recovery-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=gnss-loss.*gnss=GPS GOOD.*gnss-mode=normal"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-storage-failure-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario storage-failure
            --frames 4
    )
    set_tests_properties(
        simulator-storage-failure-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=storage-failure.*logging=NO LOG.*storage-mode=missing"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-storage-recovery-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario storage-failure
            --frames 125
    )
    set_tests_properties(
        simulator-storage-recovery-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=storage-failure.*logging=LOGGING.*storage-mode=ready.*storage-recoveries=1"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-recorded-20hz-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --gnss-rate 20
            --gnss-fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/recorded_reference_v1.csv"
            --frames 10
    )
    set_tests_properties(
        simulator-recorded-20hz-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "fixture=recorded-reference-v1.*gnss-rate=20"
            TIMEOUT 20
    )
endif()
