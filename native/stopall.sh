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
#
set -e

function stop_localstack {
  echo "Stopping localstack..."
  localstack stop
  echo "Done"
}

function stop_webserver {
  echo "Stopping webserver..."
  kill -9 $(ps aux | grep '[p]ython3 -m http.server' | awk '{print $2}')
  echo "Done"
}

function stop_asio_multipplier {
  echo "Stopping asio multiplier..."
  kill -9 $(ps aux | grep '[a]sio_multiplier' | awk '{print $2}')
  echo "Done"
}

function stop_zilliqad {
  echo "Stopping zilliqad..."
  kill -s SIGTERM $(ps aux | grep '[z]illiqad' | awk '{print $2}')
  echo "Done"
}

function stop_zilliqa {
  echo "Stopping zilliqa..."
  kill -s SIGTERM $(ps aux | grep '[z]illiqa' | awk '{print $2}')
  echo "Done"
}

function stop_upload_incr_DB {
  echo "Stopping upload_incr_DB..."
  kill -9 $(ps aux | grep '[u]pload_incr_DB' | awk '{print $2}')
  echo "Done"
}

function stop_download_incr_DB {
  echo "Stopping download_incr_DB..."
  kill -9 $(ps aux | grep '[d]download_incr_DB' | awk '{print $2}')
  echo "Done"
}

function stop_download_static_DB {
  echo "Stopping download_static_DB..."
  kill -9 $(ps aux | grep '[d]download_static_DB' | awk '{print $2}')
  echo "Done"
}




echo "Halting processes .."

result=$(stop_asio_multipplier)
result=$(stop_zilliqad)
result=$(stop_zilliqa)
result=$(stop_webserver)
result=$(stop_localstack)
result=$(stop_upload_incr_DB)
result=$(stop_download_incr_DB)
result=$(stop_download_static_DB)

echo "Done"