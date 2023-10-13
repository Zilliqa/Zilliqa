#!/bin/bash

export BUILD_DIR=../build
export RUNDIR=rundirs
python3 native.py -n 6 -d 5 -l 1 --port 30303 --websocket= --transaction-sender=0 --ds-guard=4 --shard-guard=0 --bucket=zilliqa-devnet  --origin-server=$RUNDIRS --multiplier-fanout=1 --out-dir=$RUNDIRS  --build-dir=$BUILD_DIR

