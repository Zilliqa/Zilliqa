#!/bin/bash
# Copyright (C) 2023 Zilliqa
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
# This script will start an isolated server and run the python API against it

# Find all directories containing "native"
set -e


function start_task {
  local taskname=$1
  echo "Starting task "$taskname"..."
  dirs=$(find . -type d -name "$taskname" )
  # Loop through each directory and run "start.sh"
  for dir in $dirs; do
    if [ -f "$dir/start.sh" ]; then
      echo "Running start.sh in $dir..."
      (cd "$dir" &&  chmod +x start.sh && ./start.sh &)
    else
      echo "No start.sh found in $dir"
    fi
  done
  echo "Done"
}

function start_webserver {
  echo "Starting webserver..."
  (cd rundirs &&  python3 -m http.server& )
  echo "Done"
}

function start_localstack {
  echo "Starting localstack..."
  localstack start -d
  echo "Done"
}

echo "starting web server..."
(cd rundirs &&  python3 -m http.server& )
echo "starting localstack ..."
result=$(start_localstack)
echo "starting lookups..."
result=$(start_task "*native-lookup*")
echo "starting guards..."
result=$(start_task "*native-dsguard*")
echo "starting normal..."
result=$(start_task "*native-normal*")
echo "starting seedpubs..."
result=$(start_task "*native-seedpub*")
echo "starting multiplier..."
result=$(start_task "*native-multiplier*")
echo "$result"

