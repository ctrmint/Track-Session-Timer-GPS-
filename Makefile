PYTHON ?= python3
CXX ?= c++
IDF_IMAGE ?= espressif/idf:v6.0.2
HOST_TEST_BINARY := build/host/domain_contracts_test

.PHONY: check test track-validate repo-check host-test firmware-build firmware-container-build firmware-clean issue-preview label-preview

check: test track-validate repo-check host-test

test:
	$(PYTHON) -B -m unittest discover -s tests -p 'test_*.py'

track-validate:
	$(PYTHON) -B tools/validate_tracks.py

repo-check:
	$(PYTHON) -B tools/check_repository.py

host-test:
	mkdir -p build/host
	$(CXX) -std=c++17 -Wall -Wextra -Werror -pedantic -Ifirmware/components/domain/include tests/cpp/test_domain_contracts.cpp -o $(HOST_TEST_BINARY)
	$(HOST_TEST_BINARY)

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
