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
BUILD_DIR=$(pwd)/../build
RUNDIR=$(pwd)/rundirs
NODES=6
DSMEMBERS=5
LOOKUPMEMBERS=1
DSGUARDS=4
PORT=40000


function cleanup {
echo "Cleaning up..."
  rm -rf $RUNDIR
  mkdir -p $RUNDIR
  echo "Done"
}


result=$(cleanup)
python3 native.py -n ${NODES} -d $DSMEMBERS -l ${LOOKUPMEMBERS} --port ${PORT} --websocket= --transaction-sender=0 --ds-guard=$DSGUARDS --shard-guard=0 --bucket=zilliqa-devnet  --origin-server=${RUNDIR} --multiplier-fanout=1 --out-dir=$RUNDIR  --build-dir=${BUILD_DIR}
echo "$result"
