PYTHON ?= python3
CXX ?= c++
IDF_IMAGE ?= espressif/idf:v6.0.2
HOST_TEST_BINARY := build/host/domain_contracts_test
SIMULATOR_MODEL_TEST_BINARY := build/host/simulator_model_test
SESSION_STATE_TEST_BINARY := build/host/session_state_test
SETTINGS_TEST_BINARY := build/host/settings_test
UI_FOUNDATION_TEST_BINARY := build/host/ui_foundation_test
NAVIGATION_TEST_BINARY := build/host/navigation_test
SETTINGS_EDITOR_TEST_BINARY := build/host/settings_editor_test
TRACK_DEFINITION_TEST_BINARY := build/host/track_definition_test
TRACK_MATCHING_TEST_BINARY := build/host/track_matching_test
TRACK_SELECTION_TEST_BINARY := build/host/track_selection_test
LOG_FORMAT_TEST_BINARY := build/host/log_format_test
ASYNC_LOGGER_TEST_BINARY := build/host/async_logger_test
SESSION_REVIEW_TEST_BINARY := build/host/session_review_test
DIAGNOSTICS_TEST_BINARY := build/host/diagnostics_test
ACTIVE_SESSION_TEST_BINARY := build/host/active_session_test
SIMULATOR_BUILD_DIR ?= build/simulator
SIMULATOR_IMAGE ?= track-session-timer-simulator:lvgl-9.5.0
CMAKE ?= cmake

.PHONY: check test track-validate simulator-fixture-validate repo-check host-test simulator-model-test session-state-test settings-test settings-editor-test track-definition-test track-matching-test track-selection-test log-format-test async-logger-test session-review-test diagnostics-test active-session-test ui-foundation-test navigation-test simulator-configure simulator-build simulator-test simulator-run simulator-container-image simulator-container-test simulator-clean firmware-build firmware-container-build firmware-clean issue-preview label-preview

check: test track-validate simulator-fixture-validate repo-check host-test simulator-model-test session-state-test settings-test settings-editor-test track-definition-test track-matching-test track-selection-test log-format-test async-logger-test session-review-test diagnostics-test active-session-test ui-foundation-test navigation-test

test:
	$(PYTHON) -B -m unittest discover -s tests -p 'test_*.py'

track-validate:
	$(PYTHON) -B tools/validate_tracks.py

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
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/logger/async_logger.cpp \
		firmware/components/ui/foundation.cpp firmware/components/ui/navigation.cpp \
		firmware/components/ui/presenter.cpp \
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
		firmware/components/ui/settings_editor.cpp \
		tests/cpp/test_settings_editor.cpp -o $(SETTINGS_EDITOR_TEST_BINARY)
	$(SETTINGS_EDITOR_TEST_BINARY)

track-definition-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/track/include \
		firmware/components/track/definition.cpp \
		tests/cpp/test_track_definition.cpp -o $(TRACK_DEFINITION_TEST_BINARY)
	$(TRACK_DEFINITION_TEST_BINARY) data/tracks/synthetic_test_loop.json

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
		-Ifirmware/components/ui/include \
		firmware/components/ui/active_session.cpp \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/presenter.cpp \
		tests/cpp/test_active_session.cpp -o $(ACTIVE_SESSION_TEST_BINARY)
	$(ACTIVE_SESSION_TEST_BINARY)

ui-foundation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/presenter.cpp \
		tests/cpp/test_ui_foundation.cpp -o $(UI_FOUNDATION_TEST_BINARY)
	$(UI_FOUNDATION_TEST_BINARY)

navigation-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic \
		-Ifirmware/components/domain/include \
		-Ifirmware/components/ui/include \
		firmware/components/ui/foundation.cpp \
		firmware/components/ui/navigation.cpp \
		firmware/components/ui/presenter.cpp \
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
	docker run --rm -u "$$(id -u):$$(id -g)" -e HOME=/tmp -e IDF_GIT_SAFE_DIR=/work -v "$$(pwd):/work" -w /work/firmware $(IDF_IMAGE) bash -lc "idf.py set-target esp32s3 && idf.py build"

firmware-clean:
	cd firmware && idf.py fullclean

issue-preview:
	$(PYTHON) -B tools/create_issues.py

label-preview:
	$(PYTHON) -B tools/create_labels.py
