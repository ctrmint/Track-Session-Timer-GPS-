.PHONY: test firmware-build firmware-clean issue-preview label-preview

test:
	python -m unittest discover -s tests -p 'test_*.py'

firmware-build:
	cd firmware && idf.py set-target esp32s3 && idf.py build

firmware-clean:
	cd firmware && idf.py fullclean

issue-preview:
	python tools/create_issues.py

label-preview:
	python tools/create_labels.py
