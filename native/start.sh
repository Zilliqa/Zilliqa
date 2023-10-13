#!/bin/bash
set -e
BUILD_DIR=$(pwd)/../build
RUNDIR=$(pwd)/rundirs
NODES=6
DSMEMBERS=5
LOOKUPMEMBERS=1
PORT=30303
GUARD=4

function cleanup {
echo "Cleaning up..."
  rm -rf $RUNDIR
  mkdir -p $RUNDIR
  echo "Done"
}


result=$(cleanup)
python3 native.py -n ${NODES} -d $DSMEMBERS -l ${LOOKUPMEMBERS} --port ${PORT} --websocket= --transaction-sender=0 --ds-guard=4 --shard-guard=0 --bucket=zilliqa-devnet  --origin-server=${RUNDIR} --multiplier-fanout=1 --out-dir=$RUNDIR  --build-dir=${BUILD_DIR}
echo "$result"
