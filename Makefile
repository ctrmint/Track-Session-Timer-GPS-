PYTHON ?= python3
CXX ?= c++
IDF_IMAGE ?= espressif/idf:v6.0.2
FIRMWARE_PORT ?= /dev/ttyACM0
FIRMWARE_BAUD ?= 921600
# The port is root:dialout and the container runs as the invoking user so that build
# artefacts stay user-owned, so the container needs dialout added explicitly.
DIALOUT_GID := $(shell getent group dialout | cut -d: -f3)
IDF_DOCKER := docker run --rm -u "$$(id -u):$$(id -g)" -e HOME=/tmp -e IDF_GIT_SAFE_DIR=/work -v "$$(pwd):/work" -w /work/firmware
IDF_DOCKER_TTY := $(IDF_DOCKER) --group-add $(DIALOUT_GID) --device=$(FIRMWARE_PORT)
HOST_TEST_BINARY := build/host/domain_contracts_test
SIMULATOR_MODEL_TEST_BINARY := build/host/simulator_model_test
SESSION_STATE_TEST_BINARY := build/host/session_state_test
SETTINGS_TEST_BINARY := build/host/settings_test
UI_FOUNDATION_TEST_BINARY := build/host/ui_foundation_test
NAVIGATION_TEST_BINARY := build/host/navigation_test
SETTINGS_EDITOR_TEST_BINARY := build/host/settings_editor_test
TRACK_DEFINITION_TEST_BINARY := build/host/track_definition_test
TRACK_PACK_TEST_BINARY := build/host/track_pack_test
TRACK_CAPTURE_TEST_BINARY := build/host/track_capture_test
PROJECTION_TEST_BINARY := build/host/projection_test
INTERSECTION_TEST_BINARY := build/host/intersection_test
CROSSING_VALIDATION_TEST_BINARY := build/host/crossing_validation_test
CROSSING_TIME_TEST_BINARY := build/host/crossing_time_test
LAP_STATE_MACHINE_TEST_BINARY := build/host/lap_state_machine_test
TIMING_ENGINE_TEST_BINARY := build/host/timing_engine_test
GATE_EVENT_ENGINE_TEST_BINARY := build/host/gate_event_engine_test
TRACK_MATCHING_TEST_BINARY := build/host/track_matching_test
TRACK_SELECTION_TEST_BINARY := build/host/track_selection_test
SIMULATOR_TRACK_CATALOG_TEST_BINARY := build/host/simulator_track_catalog_test
LOG_FORMAT_TEST_BINARY := build/host/log_format_test
ASYNC_LOGGER_TEST_BINARY := build/host/async_logger_test
SESSION_REVIEW_TEST_BINARY := build/host/session_review_test
DIAGNOSTICS_TEST_BINARY := build/host/diagnostics_test
ACTIVE_SESSION_TEST_BINARY := build/host/active_session_test
DISPLAY_POLICY_TEST_BINARY := build/host/display_policy_test
IMU_METER_TEST_BINARY := build/host/imu_meter_test
REST_SESSION_TEST_BINARY := build/host/rest_session_test
TRACK_CATALOG_TEST_BINARY := build/host/track_catalog_test
SHELL_NAVIGATION_TEST_BINARY := build/host/shell_navigation_test
DEVICE_MODE_TEST_BINARY := build/host/device_mode_test
GPS_ONLY_TEST_BINARY := build/host/gps_only_test
IMU_CALIBRATION_TEST_BINARY := build/host/imu_calibration_test
SESSION_URGENCY_TEST_BINARY := build/host/session_urgency_test
TIME_ROLLER_TEST_BINARY := build/host/time_roller_test
SESSION_TRIGGER_TEST_BINARY := build/host/session_trigger_test
SUMMARY_STORE_TEST_BINARY := build/host/summary_store_test
SUMMARY_FILE_TEST_BINARY := build/host/summary_file_test
REVIEW_CARDS_TEST_BINARY := build/host/review_cards_test
UBX_PARSER_TEST_BINARY := build/host/ubx_parser_test
FIX_VALIDATION_TEST_BINARY := build/host/fix_validation_test
GNSS_PIPELINE_TEST_BINARY := build/host/gnss_pipeline_test
I2C_TRANSPORT_TEST_BINARY := build/host/i2c_transport_test
GATE_CAPTURE_TEST_BINARY := build/host/gate_capture_test
GATE_SESSION_AUTOMATION_TEST_BINARY := build/host/gate_session_automation_test
SIMULATOR_BUILD_DIR ?= build/simulator
SIMULATOR_IMAGE ?= track-session-timer-simulator:lvgl-9.5.0
CMAKE ?= cmake

.PHONY: check test track-validate uk-track-pack track-pack-test simulator-track-catalog-test simulator-fixture-validate repo-check host-test simulator-model-test session-state-test settings-test settings-editor-test projection-test intersection-test crossing-validation-test crossing-time-test lap-state-machine-test timing-engine-test gate-event-engine-test track-definition-test track-capture-test track-matching-test track-selection-test log-format-test async-logger-test session-review-test diagnostics-test active-session-test display-policy-test imu-meter-test rest-session-test track-catalog-test shell-navigation-test device-mode-test gps-only-test imu-calibration-test session-urgency-test time-roller-test session-trigger-test summary-store-test summary-file-test review-cards-test ubx-parser-test fix-validation-test gnss-pipeline-test i2c-transport-test gate-capture-test gate-session-automation-test ui-foundation-test navigation-test simulator-configure simulator-build simulator-test simulator-run simulator-container-image simulator-container-test simulator-clean firmware-build firmware-container-build firmware-container-flash firmware-container-monitor firmware-container-flash-monitor firmware-container-erase firmware-device-info firmware-clean issue-preview label-preview

check: test track-validate track-pack-test simulator-track-catalog-test simulator-fixture-validate repo-check host-test simulator-model-test session-state-test settings-test settings-editor-test projection-test intersection-test crossing-validation-test crossing-time-test lap-state-machine-test timing-engine-test gate-event-engine-test track-definition-test track-capture-test track-matching-test track-selection-test log-format-test async-logger-test session-review-test diagnostics-test active-session-test display-policy-test imu-meter-test rest-session-test track-catalog-test shell-navigation-test device-mode-test gps-only-test imu-calibration-test session-urgency-test time-roller-test session-trigger-test summary-store-test summary-file-test review-cards-test ubx-parser-test fix-validation-test gnss-pipeline-test i2c-transport-test gate-capture-test gate-session-automation-test ui-foundation-test navigation-test

test:
	$(PYTHON) -B -m unittest discover -s tests -p 'test_*.py'

track-validate:
	$(PYTHON) -B tools/validate_tracks.py

uk-track-pack:
	$(PYTHON) -B tools/build_uk_track_pack.py

track-pack-test: uk-track-pack
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		firmware/components/track/definition.cpp \
		firmware/components/track/projection.cpp \
		tests/cpp/test_track_pack.cpp -o $(TRACK_PACK_TEST_BINARY)
	$(TRACK_PACK_TEST_BINARY) build/track-pack/uk

simulator-track-catalog-test: uk-track-pack
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		-Isimulator/include \
		firmware/components/track/definition.cpp \
		firmware/components/track/projection.cpp \
		simulator/src/track_catalog_loader.cpp \
		simulator/src/track_fixtures.cpp \
		tests/cpp/test_simulator_track_catalog.cpp \
		-o $(SIMULATOR_TRACK_CATALOG_TEST_BINARY)
	$(SIMULATOR_TRACK_CATALOG_TEST_BINARY) build/track-pack/uk/definitions

simulator-fixture-validate:
	$(PYTHON) -B tools/validate_simulator_fixtures.py

repo-check:
	$(PYTHON) -B tools/check_repository.py

host-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic -Ifirmware/components/domain/include tests/cpp/test_domain_contracts.cpp -o $(HOST_TEST_BINARY)
	$(HOST_TEST_BINARY)

simulator-model-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/logger/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/logger/async_logger.cpp \
		firmware/components/ui/foundation.cpp firmware/components/ui/navigation.cpp \
		firmware/components/ui/presenter.cpp firmware/components/ui/time_roller.cpp \
		simulator/src/device_backends.cpp \
		simulator/src/fixed_cell_text.cpp \
		simulator/src/scenario.cpp \
		tests/cpp/test_simulator_model.cpp -o $(SIMULATOR_MODEL_TEST_BINARY)
	$(SIMULATOR_MODEL_TEST_BINARY)

session-state-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/session/include \
		firmware/components/session/component.cpp \
		tests/cpp/test_session_controller.cpp -o $(SESSION_STATE_TEST_BINARY)
	$(SESSION_STATE_TEST_BINARY)

settings-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/settings/include \
		-Isimulator/include \
		firmware/components/settings/component.cpp \
		simulator/src/file_settings_store.cpp \
		tests/cpp/test_settings.cpp -o $(SETTINGS_TEST_BINARY)
	$(SETTINGS_TEST_BINARY)

settings-editor-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/settings/component.cpp \
		firmware/components/ui/settings_editor.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_settings_editor.cpp -o $(SETTINGS_EDITOR_TEST_BINARY)
	$(SETTINGS_EDITOR_TEST_BINARY)

projection-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		firmware/components/track/projection.cpp \
		tests/cpp/test_projection.cpp -o $(PROJECTION_TEST_BINARY)
	$(PROJECTION_TEST_BINARY)

intersection-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		-Ifirmware/components/timing/include \
		firmware/components/timing/intersection.cpp \
		tests/cpp/test_intersection.cpp -o $(INTERSECTION_TEST_BINARY)
	$(INTERSECTION_TEST_BINARY)

crossing-validation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/track/include \
		-Ifirmware/components/timing/include \
		firmware/components/timing/intersection.cpp \
		firmware/components/timing/crossing_validation.cpp \
		tests/cpp/test_crossing_validation.cpp -o $(CROSSING_VALIDATION_TEST_BINARY)
	$(CROSSING_VALIDATION_TEST_BINARY)

crossing-time-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/timing/include \
		firmware/components/timing/crossing_time.cpp \
		tests/cpp/test_crossing_time.cpp -o $(CROSSING_TIME_TEST_BINARY)
	$(CROSSING_TIME_TEST_BINARY)

lap-state-machine-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/timing/include \
		firmware/components/timing/lap_state_machine.cpp \
		tests/cpp/test_lap_state_machine.cpp -o $(LAP_STATE_MACHINE_TEST_BINARY)
	$(LAP_STATE_MACHINE_TEST_BINARY)

timing-engine-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/track/include \
		-Ifirmware/components/timing/include \
		firmware/components/track/projection.cpp \
		firmware/components/timing/intersection.cpp \
		firmware/components/timing/crossing_validation.cpp \
		firmware/components/timing/crossing_time.cpp \
		firmware/components/timing/lap_state_machine.cpp \
		firmware/components/timing/engine.cpp \
		tests/cpp/test_timing_engine.cpp -o $(TIMING_ENGINE_TEST_BINARY)
	$(TIMING_ENGINE_TEST_BINARY)

gate-event-engine-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/track/include \
		-Ifirmware/components/timing/include \
		firmware/components/track/projection.cpp \
		firmware/components/timing/intersection.cpp \
		firmware/components/timing/crossing_validation.cpp \
		firmware/components/timing/crossing_time.cpp \
		firmware/components/timing/lap_state_machine.cpp \
		firmware/components/timing/engine.cpp \
		tests/cpp/test_gate_event_engine.cpp -o $(GATE_EVENT_ENGINE_TEST_BINARY)
	$(GATE_EVENT_ENGINE_TEST_BINARY)

track-definition-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		firmware/components/track/definition.cpp \
		firmware/components/track/projection.cpp \
		tests/cpp/test_track_definition.cpp -o $(TRACK_DEFINITION_TEST_BINARY)
	$(TRACK_DEFINITION_TEST_BINARY) data/tracks/synthetic_test_loop.json

track-capture-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/track/include \
		firmware/components/track/capture.cpp \
		firmware/components/track/projection.cpp \
		tests/cpp/test_track_capture.cpp -o $(TRACK_CAPTURE_TEST_BINARY)
	$(TRACK_CAPTURE_TEST_BINARY)

track-matching-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		firmware/components/track/matching.cpp \
		tests/cpp/test_track_matching.cpp -o $(TRACK_MATCHING_TEST_BINARY)
	$(TRACK_MATCHING_TEST_BINARY)

track-selection-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/track/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/settings/component.cpp \
		firmware/components/track/definition.cpp \
		firmware/components/track/matching.cpp \
		firmware/components/track/projection.cpp \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/track_selection.cpp \
		simulator/src/track_fixtures.cpp \
		tests/cpp/test_track_selection.cpp -o $(TRACK_SELECTION_TEST_BINARY)
	$(TRACK_SELECTION_TEST_BINARY)

log-format-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/logger/include \
		firmware/components/settings/component.cpp \
		firmware/components/logger/formats.cpp \
		tests/cpp/test_log_formats.cpp -o $(LOG_FORMAT_TEST_BINARY)
	$(LOG_FORMAT_TEST_BINARY)

async-logger-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/logger/include \
		firmware/components/logger/async_logger.cpp \
		tests/cpp/test_async_logger.cpp -o $(ASYNC_LOGGER_TEST_BINARY)
	$(ASYNC_LOGGER_TEST_BINARY)

session-review-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/logger/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/settings/component.cpp \
		firmware/components/logger/formats.cpp \
		firmware/components/ui/session_review.cpp \
		simulator/src/summary_fixtures.cpp \
		tests/cpp/test_session_review.cpp -o $(SESSION_REVIEW_TEST_BINARY)
	$(SESSION_REVIEW_TEST_BINARY)

diagnostics-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/diagnostics/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/logger/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/ui/diagnostics.cpp \
		firmware/components/ui/foundation.cpp \
		simulator/src/diagnostics_fixtures.cpp \
		tests/cpp/test_diagnostics.cpp -o $(DIAGNOSTICS_TEST_BINARY)
	$(DIAGNOSTICS_TEST_BINARY)

active-session-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/active_session.cpp \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/presenter.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_active_session.cpp -o $(ACTIVE_SESSION_TEST_BINARY)
	$(ACTIVE_SESSION_TEST_BINARY)

display-policy-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/display_policy.cpp \
		tests/cpp/test_display_policy.cpp -o $(DISPLAY_POLICY_TEST_BINARY)
	$(DISPLAY_POLICY_TEST_BINARY)

imu-meter-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/ui/imu_meter.cpp \
		simulator/src/imu_fixtures.cpp \
		tests/cpp/test_imu_meter.cpp -o $(IMU_METER_TEST_BINARY)
	$(IMU_METER_TEST_BINARY)

rest-session-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/session/include \
		-Ifirmware/components/ui/include \
		firmware/components/session/component.cpp \
		firmware/components/ui/rest_session.cpp \
		tests/cpp/test_rest_session.cpp -o $(REST_SESSION_TEST_BINARY)
	$(REST_SESSION_TEST_BINARY)

track-catalog-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/catalog/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/timing/include \
		-Ifirmware/components/track/include \
		-Isimulator/include \
		firmware/components/catalog/track_catalog.cpp \
		firmware/components/catalog/track_loader.cpp \
		firmware/components/settings/component.cpp \
		firmware/components/timing/crossing_time.cpp \
		firmware/components/timing/crossing_validation.cpp \
		firmware/components/timing/engine.cpp \
		firmware/components/timing/intersection.cpp \
		firmware/components/timing/lap_state_machine.cpp \
		firmware/components/timing/settings_adapter.cpp \
		firmware/components/track/definition.cpp \
		firmware/components/track/projection.cpp \
		simulator/src/file_track_definition_store.cpp \
		tests/cpp/test_track_catalog.cpp -o $(TRACK_CATALOG_TEST_BINARY)
	$(TRACK_CATALOG_TEST_BINARY) data/tracks/synthetic_test_loop.json

# These two cover firmware domain logic and need no LVGL. They previously built only
# inside the simulator suite, so their coverage depended on a build path that is no
# longer being developed.
gate-capture-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/track/include \
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/track/capture.cpp \
		firmware/components/track/definition.cpp \
		firmware/components/track/matching.cpp \
		firmware/components/track/projection.cpp \
		firmware/components/ui/gate_capture.cpp \
		simulator/src/file_track_definition_store.cpp \
		simulator/src/track_fixtures.cpp \
		tests/cpp/test_gate_capture.cpp -o $(GATE_CAPTURE_TEST_BINARY)
	$(GATE_CAPTURE_TEST_BINARY)

gate-session-automation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/session/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/timing/include \
		-Ifirmware/components/track/include \
		-Isimulator/include \
		firmware/components/session/component.cpp \
		firmware/components/session/gate_automation.cpp \
		firmware/components/settings/component.cpp \
		firmware/components/timing/settings_adapter.cpp \
		firmware/components/track/definition.cpp \
		firmware/components/track/projection.cpp \
		simulator/src/track_fixtures.cpp \
		tests/cpp/test_gate_session_automation.cpp \
		-o $(GATE_SESSION_AUTOMATION_TEST_BINARY)
	$(GATE_SESSION_AUTOMATION_TEST_BINARY)

i2c-transport-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/gnss/include \
		firmware/components/gnss/i2c_transport.cpp \
		tests/cpp/test_i2c_transport.cpp -o $(I2C_TRANSPORT_TEST_BINARY)
	$(I2C_TRANSPORT_TEST_BINARY)

gnss-pipeline-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/gnss/include \
		firmware/components/gnss/ubx.cpp \
		firmware/components/gnss/fix_validation.cpp \
		firmware/components/gnss/pipeline.cpp \
		tests/cpp/test_gnss_pipeline.cpp -o $(GNSS_PIPELINE_TEST_BINARY)
	$(GNSS_PIPELINE_TEST_BINARY)

fix-validation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/gnss/include \
		-Ifirmware/components/timing/include \
		-Ifirmware/components/track/include \
		firmware/components/gnss/fix_validation.cpp \
		tests/cpp/test_fix_validation.cpp -o $(FIX_VALIDATION_TEST_BINARY)
	$(FIX_VALIDATION_TEST_BINARY)

ubx-parser-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/gnss/include \
		firmware/components/gnss/ubx.cpp \
		tests/cpp/test_ubx_parser.cpp -o $(UBX_PARSER_TEST_BINARY)
	$(UBX_PARSER_TEST_BINARY)

review-cards-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/logger/include \
		-Ifirmware/components/ui/include \
		firmware/components/settings/component.cpp \
		firmware/components/logger/formats.cpp \
		firmware/components/ui/review_cards.cpp \
		tests/cpp/test_review_cards.cpp -o $(REVIEW_CARDS_TEST_BINARY)
	$(REVIEW_CARDS_TEST_BINARY)

summary-file-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/logger/include \
		firmware/components/settings/component.cpp \
		firmware/components/logger/formats.cpp \
		firmware/components/logger/memory_summary_store.cpp \
		firmware/components/logger/summary_frame.cpp \
		firmware/components/logger/file_summary_store.cpp \
		tests/cpp/test_summary_file.cpp -o $(SUMMARY_FILE_TEST_BINARY)
	$(SUMMARY_FILE_TEST_BINARY)

summary-store-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/logger/include \
		firmware/components/settings/component.cpp \
		firmware/components/logger/formats.cpp \
		firmware/components/logger/memory_summary_store.cpp \
		tests/cpp/test_summary_store.cpp -o $(SUMMARY_STORE_TEST_BINARY)
	$(SUMMARY_STORE_TEST_BINARY)

session-trigger-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/settings/component.cpp \
		firmware/components/ui/session_trigger.cpp \
		firmware/components/ui/value_picker.cpp \
		firmware/components/ui/time_roller.cpp \
		tests/cpp/test_session_trigger.cpp -o $(SESSION_TRIGGER_TEST_BINARY)
	$(SESSION_TRIGGER_TEST_BINARY)

time-roller-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/settings/component.cpp \
		firmware/components/ui/time_roller.cpp \
		tests/cpp/test_time_roller.cpp -o $(TIME_ROLLER_TEST_BINARY)
	$(TIME_ROLLER_TEST_BINARY)

session-urgency-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/active_session.cpp \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/presenter.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_session_urgency.cpp -o $(SESSION_URGENCY_TEST_BINARY)
	$(SESSION_URGENCY_TEST_BINARY)

imu-calibration-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/board/include \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/imu/include \
		firmware/components/imu/calibration.cpp \
		tests/cpp/test_imu_calibration.cpp -o $(IMU_CALIBRATION_TEST_BINARY)
	$(IMU_CALIBRATION_TEST_BINARY)

gps-only-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/gps_only.cpp \
		tests/cpp/test_gps_only.cpp -o $(GPS_ONLY_TEST_BINARY)
	$(GPS_ONLY_TEST_BINARY)

device-mode-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/settings/component.cpp \
		firmware/components/ui/device_mode.cpp \
		firmware/components/ui/value_picker.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_device_mode.cpp -o $(DEVICE_MODE_TEST_BINARY)
	$(DEVICE_MODE_TEST_BINARY)

shell-navigation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/input.cpp \
		firmware/components/ui/navigation.cpp \
		firmware/components/ui/shell_navigation.cpp \
		tests/cpp/test_shell_navigation.cpp -o $(SHELL_NAVIGATION_TEST_BINARY)
	$(SHELL_NAVIGATION_TEST_BINARY)

ui-foundation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/presenter.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_ui_foundation.cpp -o $(UI_FOUNDATION_TEST_BINARY)
	$(UI_FOUNDATION_TEST_BINARY)

navigation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/settings/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/navigation.cpp \
		firmware/components/ui/presenter.cpp firmware/components/ui/time_roller.cpp \
		tests/cpp/test_navigation.cpp -o $(NAVIGATION_TEST_BINARY)
	$(NAVIGATION_TEST_BINARY)

simulator-configure:
	$(CMAKE) -S simulator -B $(SIMULATOR_BUILD_DIR) -G Ninja

simulator-build: simulator-configure
	$(CMAKE) --build $(SIMULATOR_BUILD_DIR)

simulator-test: simulator-build
	ctest --test-dir $(SIMULATOR_BUILD_DIR) --output-on-failure

simulator-run: simulator-build
	$(SIMULATOR_BUILD_DIR)/track_timer_simulator --scenario ready

simulator-container-image:
	docker build -t $(SIMULATOR_IMAGE) simulator

simulator-container-test: simulator-container-image
	docker run --rm -u "$$(id -u):$$(id -g)" -e HOME=/tmp -v "$$(pwd):/work" -w /work $(SIMULATOR_IMAGE) make simulator-test

simulator-clean:
	$(CMAKE) -E rm -rf $(SIMULATOR_BUILD_DIR)

firmware-build:
	cd firmware && idf.py set-target esp32s3 && idf.py build

firmware-container-build:
	$(IDF_DOCKER) $(IDF_IMAGE) bash -lc "idf.py set-target esp32s3 && idf.py build"

# Read-only identification. Resets the chip but writes nothing.
firmware-device-info:
	$(IDF_DOCKER_TTY) $(IDF_IMAGE) bash -lc "esptool --port $(FIRMWARE_PORT) --chip esp32s3 chip-id && esptool --port $(FIRMWARE_PORT) --chip esp32s3 flash-id"

firmware-container-flash: firmware-container-build
	$(IDF_DOCKER_TTY) $(IDF_IMAGE) bash -lc "idf.py -p $(FIRMWARE_PORT) -b $(FIRMWARE_BAUD) flash"

firmware-container-monitor:
	$(IDF_DOCKER_TTY) -it $(IDF_IMAGE) bash -lc "idf.py -p $(FIRMWARE_PORT) monitor"

firmware-container-flash-monitor: firmware-container-build
	$(IDF_DOCKER_TTY) -it $(IDF_IMAGE) bash -lc "idf.py -p $(FIRMWARE_PORT) -b $(FIRMWARE_BAUD) flash monitor"

# Destructive: wipes the whole 16 MB part, including NVS-stored settings.
firmware-container-erase:
	$(IDF_DOCKER_TTY) $(IDF_IMAGE) bash -lc "idf.py -p $(FIRMWARE_PORT) erase-flash"

firmware-clean:
	cd firmware && idf.py fullclean

issue-preview:
	$(PYTHON) -B tools/create_issues.py

label-preview:
	$(PYTHON) -B tools/create_labels.py
