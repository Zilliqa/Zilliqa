#!/bin/bash
# Copyright (C) 2024 Zilliqa
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
# This script is used for making image locally and pushing to private registry
#!/bin/bash

# Set the expected directory
# Usage : ./scripts/run_eth_transaction_count_api_test.sh localdev2
expected_dir="EvmAcceptanceTests"

current_dir=$(basename "$(pwd)")

if [ "$current_dir" != "$expected_dir" ]; then
    echo "Error: You are not in the $expected_dir directory."
    echo "Please change to the correct directory and try again."
    exit 1
fi

if [ -z "$1" ]; then
    echo "Error: No network specified. Usage: $0 <network>"
    exit 1
fi
network=$1

start_test() {
    echo "Starting $1..."
    npx hardhat test "$2" --network "$network" &
    eval "$3=\$!"
}

echo "Starting tests..."

start_test "Deploy dummy contract" "test/eth_transaction_count_api/deployDummyContract.ts" "first_pid"
wait $first_pid
sleep 1
start_test "first test" "test/eth_transaction_count_api/txn.ts" "second_pid"
sleep 1
start_test "second test" "test/eth_transaction_count_api/txn.ts" "third_pid"
sleep 1
start_test "third test" "test/eth_transaction_count_api/txn.ts" "fourth_pid"
sleep 1
start_test "fourth test" "test/eth_transaction_count_api/txn.ts" "fifth_pid"

wait $second_pid
wait $third_pid
wait $fourth_pid
wait $fifth_pid

FILE_PATH="$PWD/test/eth_transaction_count_api/contractAddress.txt"
echo "FILE_PATH: $FILE_PATH"

if [ -f "$FILE_PATH" ]; then
    rm "$FILE_PATH" && echo "Contract file deleted successfully." || echo "Failed to delete contract file."
else
    echo "No contract file to delete."
fi

echo "All tests completed."
