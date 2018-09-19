#!/bin/bash
#
# This script is dedicated for CI use
#

set -e

# assume that it is run from project root directory
bash <(curl -s https://codecov.io/bash) -g '/usr/**' -x gcov-5 || echo "Codecov did not collect coverage reports"
