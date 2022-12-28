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

# fix to make script work locally, please remove and fix to original intent.
if [ -f /scilla/0 ]
  then
    ls /scilla/0/
fi

re="\\bNOCI\\b"
if [[ "$TRAVIS_COMMIT_MESSAGE" =~ $re ]]
then
    exit 0
fi

${SHELL} -x ./build.sh debug tests coverage

