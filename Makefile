PYTHON ?= python3
CXX ?= c++
IDF_IMAGE ?= espressif/idf:v6.0.2
HOST_TEST_BINARY := build/host/domain_contracts_test
SIMULATOR_MODEL_TEST_BINARY := build/host/simulator_model_test
SESSION_STATE_TEST_BINARY := build/host/session_state_test
SIMULATOR_BUILD_DIR ?= build/simulator
SIMULATOR_IMAGE ?= track-session-timer-simulator:lvgl-9.5.0
CMAKE ?= cmake

.PHONY: check test track-validate simulator-fixture-validate repo-check host-test simulator-model-test session-state-test simulator-configure simulator-build simulator-test simulator-run simulator-container-image simulator-container-test simulator-clean firmware-build firmware-container-build firmware-clean issue-preview label-preview

check: test track-validate simulator-fixture-validate repo-check host-test simulator-model-test session-state-test

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
		-Ifirmware/components/ui/include \
		-Isimulator/include \
		firmware/components/ui/presenter.cpp simulator/src/device_backends.cpp \
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

simulator-configure:
	$(CMAKE) -S simulator -B $(SIMULATOR_BUILD_DIR) -G Ninja

simulator-build: simulator-configure
	$(CMAKE) --build $(SIMULATOR_BUILD_DIR)

simulator-test: simulator-build
	ctest --test-dir $(SIMULATOR_BUILD_DIR) --output-on-failure

simulator-run: simulator-build
	$(SIMULATOR_BUILD_DIR)/track_timer_simulator --scenario active

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
