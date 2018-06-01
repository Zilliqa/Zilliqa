#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

# assume that it is run from project root directory
lcov --gcov-tool gcov-5 --directory . --capture --output-file coverage.info
lcov --gcov-tool gcov-5 --remove coverage.info '/usr/*' --output-file coverage.info
lcov --gcov-tool gcov-5 --list coverage.info
bash <(curl -s https://codecov.io/bash) -g '/usr/**' -x gcov-5 || echo "Codecov did not collect coverage reports"
