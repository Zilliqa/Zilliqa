#!/bin/bash
# script to fire off native build
ZILLIQA_ROOT=/home/stephen/dev/Zilliqa
# kick off the native localinit.py script
python3 $ZILLIQA_ROOT/scripts/localinit.py -n 6 -d 5 -l 1 --port 30303 \
--websocket= --transaction-sender=0 --ds-guard=4 --shard-guard=0  --bucket=zilliqa-devnet \
--conf-dir=$ZILLIQA_ROOT/../testnet/localdev --origin-server=$(pwd) \
--multiplier-fanout=1 --out-dir=$(pwd) --testnet=localdev --build-dir=$ZILLIQA_ROOT/build
