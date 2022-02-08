#!/bin/bash
# Copyright (C) 2019 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# This script is dedicated for CI use
#

set -e

# Create lcov report
# capture coverage info
lcov --directory . --capture --output-file coverage.info
# filter out system and extra files.
# To also not include test code in coverage add them with full path to the patterns: '*/tests/*'
lcov --remove coverage.info '/usr/*' "${HOME}"'/.cache/*' --output-file coverage.info
# output coverage data for debugging (optional)
lcov --list coverage.info
# Uploading to CodeCov
# '-f' specifies file(s) to use and disables manual coverage gathering and file search which has already been done above
bash <(curl -s https://codecov.io/bash) -f coverage.info || echo "Codecov did not collect coverage reports"
