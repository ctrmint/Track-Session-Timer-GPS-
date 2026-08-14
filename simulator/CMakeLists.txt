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
    ../firmware/components/ui/active_session.cpp
    ../firmware/components/ui/foundation.cpp
    ../firmware/components/ui/diagnostics.cpp
    ../firmware/components/ui/display_policy.cpp
    ../firmware/components/ui/imu_meter.cpp
    ../firmware/components/ui/navigation.cpp
    ../firmware/components/ui/presenter.cpp
    ../firmware/components/ui/rest_session.cpp
    ../firmware/components/ui/session_review.cpp
    ../firmware/components/ui/settings_editor.cpp
    ../firmware/components/ui/track_selection.cpp
    ../firmware/components/settings/component.cpp
    ../firmware/components/session/component.cpp
    ../firmware/components/logger/async_logger.cpp
    ../firmware/components/logger/formats.cpp
    ../firmware/components/track/definition.cpp
    ../firmware/components/track/matching.cpp
    ../firmware/components/track/projection.cpp
    ../firmware/components/timing/intersection.cpp
    ../firmware/components/timing/crossing_validation.cpp
    src/device_backends.cpp
    src/diagnostics_fixtures.cpp
    src/display_fixtures.cpp
    src/file_settings_store.cpp
    src/fixed_cell_text.cpp
    src/imu_fixtures.cpp
    src/scenario.cpp
    src/summary_fixtures.cpp
    src/track_fixtures.cpp
)
target_include_directories(
    track_timer_simulator_model
    PUBLIC
        include
        ../firmware/components/board/include
        ../firmware/components/diagnostics/include
        ../firmware/components/domain/include
        ../firmware/components/logger/include
        ../firmware/components/settings/include
        ../firmware/components/session/include
        ../firmware/components/track/include
        ../firmware/components/timing/include
        ../firmware/components/ui/include
)
target_compile_features(track_timer_simulator_model PUBLIC cxx_std_17)

add_library(
    track_timer_lvgl_ui
    ../firmware/components/ui/diagnostics_screen.cpp
    ../firmware/components/ui/g_meter_screen.cpp
    ../firmware/components/ui/lvgl_visual_system.cpp
    ../firmware/components/ui/ready_screen.cpp
    ../firmware/components/ui/session_review_screen.cpp
    ../firmware/components/ui/setup_menu_screen.cpp
    ../firmware/components/ui/settings_screen.cpp
    ../firmware/components/ui/track_selection_screen.cpp
)
target_include_directories(
    track_timer_lvgl_ui
    PUBLIC
        ../firmware/components/board/include
        ../firmware/components/diagnostics/include
        ../firmware/components/domain/include
        ../firmware/components/logger/include
        ../firmware/components/settings/include
        ../firmware/components/track/include
        ../firmware/components/ui/include
)
target_compile_features(track_timer_lvgl_ui PUBLIC cxx_std_17)
target_link_libraries(track_timer_lvgl_ui PUBLIC lvgl::lvgl)

add_executable(
    track_timer_simulator
    src/application_screen.cpp
    src/device_screen.cpp
    src/fixed_cell_label.cpp
    src/main.cpp
    src/rest_screen.cpp
)
target_include_directories(track_timer_simulator PRIVATE src)
target_link_libraries(
    track_timer_simulator
    PRIVATE
        track_timer_simulator_model
        track_timer_lvgl_ui
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
    target_compile_options(
        track_timer_lvgl_ui
        PRIVATE -Wall -Wextra -Werror -pedantic
    )
endif()

if(BUILD_TESTING)
    add_executable(
        track_timer_rest_session_test
        ../tests/cpp/test_rest_session.cpp
    )
    target_link_libraries(
        track_timer_rest_session_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_rest_session_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-rest-session-model
        COMMAND track_timer_rest_session_test
    )
    set_tests_properties(
        simulator-rest-session-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Session-driven overtime, completion, guarded rest skip, and fault independence passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_imu_meter_test
        ../tests/cpp/test_imu_meter.cpp
    )
    target_link_libraries(
        track_timer_imu_meter_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_imu_meter_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-imu-meter-model
        COMMAND track_timer_imu_meter_test
    )
    set_tests_properties(
        simulator-imu-meter-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Bounded G-meter peaks, orientation, reset guard, and IMU states passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_display_policy_test
        ../tests/cpp/test_display_policy.cpp
    )
    target_link_libraries(
        track_timer_display_policy_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_display_policy_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-display-policy-model
        COMMAND track_timer_display_policy_test
    )
    set_tests_properties(
        simulator-display-policy-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Deterministic brightness, orientation, idle dim, wake, and AMOLED shift policy passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_active_session_test
        ../tests/cpp/test_active_session.cpp
    )
    target_link_libraries(
        track_timer_active_session_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_active_session_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-active-session-model
        COMMAND track_timer_active_session_test
    )
    set_tests_properties(
        simulator-active-session-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Deterministic lap feedback and hold-confirm stop controls passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_diagnostics_test
        ../tests/cpp/test_diagnostics.cpp
    )
    target_link_libraries(
        track_timer_diagnostics_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_diagnostics_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-diagnostics-model
        COMMAND track_timer_diagnostics_test
    )
    set_tests_properties(
        simulator-diagnostics-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Immutable diagnostics pages and normal/degraded/missing/recovery states passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_session_review_test
        ../tests/cpp/test_session_review.cpp
    )
    target_link_libraries(
        track_timer_session_review_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_session_review_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-session-review-model
        COMMAND track_timer_session_review_test
    )
    set_tests_properties(
        simulator-session-review-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Bounded session review paging, emphasis, and failure states passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_async_logger_test
        ../tests/cpp/test_async_logger.cpp
    )
    target_link_libraries(
        track_timer_async_logger_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_async_logger_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-async-logger
        COMMAND track_timer_async_logger_test
    )
    set_tests_properties(
        simulator-async-logger
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Bounded async logging, failure visibility, and 20/25 Hz stress passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_track_selection_test
        ../tests/cpp/test_track_selection.cpp
    )
    target_link_libraries(
        track_timer_track_selection_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_track_selection_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-track-selection
        COMMAND track_timer_track_selection_test
    )
    set_tests_properties(
        simulator-track-selection
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Track selection states, persistence, and active-session lock passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_track_matching_test
        ../tests/cpp/test_track_matching.cpp
    )
    target_link_libraries(
        track_timer_track_matching_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_track_matching_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-track-geofence-matching
        COMMAND track_timer_track_matching_test
    )
    set_tests_properties(
        simulator-track-geofence-matching
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Offline track geofence suggestion and ambiguity decisions passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_projection_test
        ../tests/cpp/test_projection.cpp
    )
    target_link_libraries(
        track_timer_projection_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_projection_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-circuit-local-projection
        COMMAND track_timer_projection_test
    )
    set_tests_properties(
        simulator-circuit-local-projection
        PROPERTIES
            PASS_REGULAR_EXPRESSION "WGS84 circuit-local projection reference vectors passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_intersection_test
        ../tests/cpp/test_intersection.cpp
    )
    target_link_libraries(
        track_timer_intersection_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_intersection_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-segment-intersection
        COMMAND track_timer_intersection_test
    )
    set_tests_properties(
        simulator-segment-intersection
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Scale-aware half-open segment intersection vectors passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_crossing_validation_test
        ../tests/cpp/test_crossing_validation.cpp
    )
    target_link_libraries(
        track_timer_crossing_validation_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_crossing_validation_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-crossing-validation
        COMMAND track_timer_crossing_validation_test
    )
    set_tests_properties(
        simulator-crossing-validation
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Directed crossing speed, heading, and GNSS-quality validation passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_track_definition_test
        ../tests/cpp/test_track_definition.cpp
    )
    target_link_libraries(
        track_timer_track_definition_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_track_definition_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()
    add_test(
        NAME simulator-versioned-track-definition
        COMMAND track_timer_track_definition_test
            "${CMAKE_CURRENT_SOURCE_DIR}/../data/tracks/synthetic_test_loop.json"
    )
    set_tests_properties(
        simulator-versioned-track-definition
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Versioned track loading, projection, hashing, and failure isolation passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_settings_editor_test
        ../tests/cpp/test_settings_editor.cpp
    )
    target_link_libraries(
        track_timer_settings_editor_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_settings_editor_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-settings-editors
        COMMAND track_timer_settings_editor_test
    )
    set_tests_properties(
        simulator-settings-editors
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Settings editor options, boundaries, defaults, and safe saves passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_navigation_test
        ../tests/cpp/test_navigation.cpp
    )
    target_link_libraries(
        track_timer_navigation_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_navigation_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-navigation-model
        COMMAND track_timer_navigation_test
    )
    set_tests_properties(
        simulator-navigation-model
        PROPERTIES
            PASS_REGULAR_EXPRESSION "Ready navigation transitions and degraded timer path passed"
            TIMEOUT 20
    )

    add_executable(
        track_timer_ui_foundation_test
        ../tests/cpp/test_ui_foundation.cpp
    )
    target_link_libraries(
        track_timer_ui_foundation_test
        PRIVATE track_timer_simulator_model
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_ui_foundation_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-ui-foundation
        COMMAND track_timer_ui_foundation_test
    )
    set_tests_properties(
        simulator-ui-foundation
        PROPERTIES
            PASS_REGULAR_EXPRESSION "UI foundation budgets, contrast, cues, and metrics passed"
            TIMEOUT 20
    )

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
            track_timer_lvgl_ui
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

    add_executable(
        track_timer_application_screen_test
        ../tests/cpp/test_application_screen.cpp
        src/application_screen.cpp
        src/device_screen.cpp
        src/fixed_cell_label.cpp
        src/rest_screen.cpp
    )
    target_include_directories(track_timer_application_screen_test PRIVATE src)
    target_link_libraries(
        track_timer_application_screen_test
        PRIVATE
            track_timer_simulator_model
            track_timer_lvgl_ui
            lvgl::lvgl
            SDL2::SDL2
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            track_timer_application_screen_test
            PRIVATE -Wall -Wextra -Werror -pedantic
        )
    endif()

    add_test(
        NAME simulator-ready-navigation
        COMMAND track_timer_application_screen_test
    )
    set_tests_properties(
        simulator-ready-navigation
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "Ready dashboard controls and application destinations passed"
            TIMEOUT 20
    )
    set_tests_properties(
        simulator-gnss-indicator
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "GPS indicator symbol, text cues, colours, and position passed"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-active-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --trackday-mode disabled
            --frames 4
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/active.ppm"
    )

    add_test(
        NAME simulator-trackday-mode-screen
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --trackday-mode enabled
            --frames 12
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/trackday-mode.ppm"
    )
    set_tests_properties(
        simulator-trackday-mode-screen
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=active.*current= session=15:26.*trackday-mode=enabled.*trackday-estimate=9.3 LAPS.*lap-feedback=none"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-ready-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario ready
            --frames 4
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/ready.ppm"
    )
    add_test(
        NAME simulator-settings-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario ready
            --screen settings
            --frames 4
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/settings.ppm"
    )
    set_tests_properties(
        simulator-settings-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=ready.*screen=setup.*setup-page=settings.*display-buffer-bytes=96000"
            TIMEOUT 20
    )

    add_test(
        NAME simulator-setup-menu-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario ready
            --screen setup
            --frames 4
    )
    set_tests_properties(
        simulator-setup-menu-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=ready.*screen=setup.*setup-page=menu"
            TIMEOUT 20
    )

    function(add_track_state_smoke fixture expected)
        add_test(
            NAME "simulator-track-${fixture}-screen"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --screen tracks
                --track-state "${fixture}"
                --frames 4
        )
        set_tests_properties(
            "simulator-track-${fixture}-screen"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "scenario=ready.*screen=setup.*setup-page=tracks.*track-state=${expected}"
                TIMEOUT 20
        )
    endfunction()

    add_track_state_smoke(selected selected)
    add_track_state_smoke(missing selected-track-missing)
    add_track_state_smoke(invalid invalid-catalog)
    add_track_state_smoke(ambiguous ambiguous)
    add_track_state_smoke(suggested suggested)

    function(add_review_state_smoke fixture expected)
        add_test(
            NAME "simulator-review-${fixture}-screen"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --screen review
                --review-state "${fixture}"
                --frames 4
        )
        set_tests_properties(
            "simulator-review-${fixture}-screen"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "scenario=ready.*screen=review.*review-state=${expected}.*review-fixture=${fixture}"
                TIMEOUT 20
        )
    endfunction()

    add_review_state_smoke(complete ready)
    add_review_state_smoke(partial partial-log)
    add_review_state_smoke(empty empty)
    add_review_state_smoke(missing storage-unavailable)
    add_review_state_smoke(corrupt corrupt)
    add_review_state_smoke(unsupported unsupported-version)

    function(add_diagnostics_state_smoke fixture expected)
        add_test(
            NAME "simulator-diagnostics-${fixture}-screen"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --screen diagnostics
                --diagnostics-state "${fixture}"
                --frames 4
        )
        set_tests_properties(
            "simulator-diagnostics-${fixture}-screen"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "scenario=ready.*screen=diagnostics.*diagnostics-state=${expected}.*diagnostics-fixture=${fixture}"
                TIMEOUT 20
        )
    endfunction()

    add_diagnostics_state_smoke(normal normal)
    add_diagnostics_state_smoke(degraded degraded)
    add_diagnostics_state_smoke(missing missing)
    add_diagnostics_state_smoke(recovery recovered)

    function(add_imu_state_smoke fixture expected frames)
        add_test(
            NAME "simulator-imu-${fixture}-screen"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --screen g-meter
                --imu-state "${fixture}"
                --frames "${frames}"
                --snapshot "${CMAKE_CURRENT_BINARY_DIR}/imu-${fixture}.ppm"
        )
        set_tests_properties(
            "simulator-imu-${fixture}-screen"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "screen=setup.*setup-page=g-meter.*imu-fixture=${fixture}.*imu-state=${expected}"
                TIMEOUT 20
        )
    endfunction()

    add_imu_state_smoke(normal ready 25)
    add_imu_state_smoke(calibration calibrating 4)
    add_imu_state_smoke(failure unavailable 4)
    add_imu_state_smoke(partial partial 4)
    add_imu_state_smoke(recovery recovered 25)

    add_test(
        NAME simulator-imu-loss-does-not-stop-timing
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --imu-state failure
            --trackday-mode disabled
            --frames 25
    )
    set_tests_properties(
        simulator-imu-loss-does-not-stop-timing
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=active.*current=1:43.638.*session=15:26.*imu-fixture=failure.*imu-state=unavailable"
            TIMEOUT 20
    )

    function(add_workflow_state_smoke fixture expected)
        add_test(
            NAME "simulator-workflow-${fixture}"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --workflow-state "${fixture}"
                --trackday-mode disabled
                --frames 4
                --snapshot "${CMAKE_CURRENT_BINARY_DIR}/workflow-${fixture}.ppm"
        )
        set_tests_properties(
            "simulator-workflow-${fixture}"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "${expected}"
                TIMEOUT 20
        )
    endfunction()

    add_workflow_state_smoke(overtime "session=\\+00:05.*screen=active.*workflow-fixture=overtime.*workflow-state=overtime")
    add_workflow_state_smoke(completion "screen=review.*review-state=ready.*workflow-fixture=completion.*workflow-state=review")
    add_workflow_state_smoke(rest "screen=rest.*workflow-fixture=rest.*workflow-state=rest")

    function(add_display_state_smoke fixture expected)
        add_test(
            NAME "simulator-display-${fixture}-policy"
            COMMAND track_timer_simulator
                --headless
                --scenario ready
                --display-state "${fixture}"
                --frames 4
                --snapshot "${CMAKE_CURRENT_BINARY_DIR}/display-${fixture}.ppm"
        )
        set_tests_properties(
            "simulator-display-${fixture}-policy"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "display-state=${fixture}.*${expected}"
                TIMEOUT 20
        )
    endfunction()

    add_display_state_smoke(day "display-profile=day.*display-brightness=100.*display-orientation=0-deg.*display-dimmed=no")
    add_display_state_smoke(night "display-profile=night.*display-brightness=50.*display-orientation=0-deg.*display-dimmed=no")
    add_display_state_smoke(rotated "display-profile=day.*display-brightness=100.*display-orientation=90-deg.*display-dimmed=no")

    add_test(
        NAME simulator-display-dimmed-policy
        COMMAND track_timer_simulator
            --headless
            --scenario ready
            --display-state dimmed
            --frames 7
            --frame-ms 10000
            --snapshot "${CMAKE_CURRENT_BINARY_DIR}/display-dimmed.ppm"
    )
    set_tests_properties(
        simulator-display-dimmed-policy
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "display-state=dimmed.*display-brightness=25.*display-orientation=0-deg.*display-dimmed=yes.*display-shift="
            TIMEOUT 20
    )

    add_test(
        NAME simulator-display-active-policy-lock
        COMMAND track_timer_simulator
            --headless
            --scenario active
            --display-state dimmed
            --trackday-mode disabled
            --frames 7
            --frame-ms 10000
    )
    set_tests_properties(
        simulator-display-active-policy-lock
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=active.*display-state=dimmed.*display-brightness=100.*display-orientation=0-deg.*display-dimmed=no.*display-shift=0,0"
            TIMEOUT 20
    )
    set_tests_properties(
        simulator-ready-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=ready.*screen=ready.*display-buffer-bytes=96000"
            TIMEOUT 20
    )
    set_tests_properties(
        simulator-active-smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
            PASS_REGULAR_EXPRESSION "scenario=active.*render-frames=[1-9][0-9]*.*lvgl-maximum-bytes=[1-9][0-9]*.*display-buffer-bytes=96000"
            TIMEOUT 20
    )

    function(add_lap_feedback_smoke scenario expected)
        add_test(
            NAME "simulator-${scenario}-feedback"
            COMMAND track_timer_simulator
                --headless
                --scenario "${scenario}"
                --trackday-mode disabled
                --frames 8
                --snapshot "${CMAKE_CURRENT_BINARY_DIR}/${scenario}.ppm"
        )
        set_tests_properties(
            "simulator-${scenario}-feedback"
            PROPERTIES
                ENVIRONMENT "SDL_VIDEODRIVER=dummy"
                PASS_REGULAR_EXPRESSION "scenario=${scenario}.*lap-feedback=${expected}"
                TIMEOUT 20
        )
    endfunction()

    add_lap_feedback_smoke(lap-faster faster)
    add_lap_feedback_smoke(lap-slower slower)
    add_lap_feedback_smoke(lap-unavailable-best unavailable-best)

    add_test(
        NAME simulator-gnss-loss-smoke
        COMMAND track_timer_simulator
            --headless
            --scenario gnss-loss
            --trackday-mode disabled
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
            --trackday-mode disabled
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
            --trackday-mode disabled
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
            --trackday-mode disabled
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
            --trackday-mode disabled
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
