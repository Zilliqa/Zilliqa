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
# This script will start an isolated server and run the python API against it
#

# Firstly determine if the user is running this locally, or is this in the CI

RUNNING_LOCALLY=1

if [[ $# -ne 1 ]];
then
    echo 'Assuming user is running on own system' >&2
else
    case $1 in
        "--setup-env"|"--SETUP-ENV")  # Ok
            RUNNING_LOCALLY=0
            ;;
        *)
            # The wrong first argument.
            echo '--setup-env or nothing' >&2
    esac
fi

if [[ "$RUNNING_LOCALLY" == 1 ]]; then
    echo "You are running the script locally. This is just for CI to run and you can do it yourself by running the test in tests/EvmAcceptanceTests against isolated server."
    echo ""
else
    echo "The CI is running this script."
    # Install dependencies silently on the CI server

    echo "NOT starting isolated server - it should be running already"
    ps -e | grep isolated

    # install dependencies
    apt update
    apt -y install curl dirmngr apt-transport-https lsb-release ca-certificates
    curl -sL https://deb.nodesource.com/setup_14.x | bash -
    apt -y install nodejs
    node --version
    pwd
    cd tests/EvmAcceptanceTests/
    npm install
    npm install --save-dev "@ethersproject/providers@^5.4.7" "@nomicfoundation/hardhat-network-helpers@^1.0.0" "@nomicfoundation/hardhat-chai-matchers@^1.0.0" "@nomiclabs/hardhat-ethers@^2.0.0" "@nomiclabs/hardhat-etherscan@^3.0.0" "@types/chai@^4.2.0" "@types/mocha@^9.1.0" "@typechain/ethers-v5@^10.1.0" "@typechain/hardhat@^6.1.2" "chai@^4.2.0" "ethers@^5.4.7" "hardhat-gas-reporter@^1.0.8" "solidity-coverage@^0.7.21" "ts-node@>=8.0.0" "typechain@^8.1.0" "typescript@>=4.5.0"

    echo "sleeping..."
    sleep 8000

    npx hardhat test

    retVal=$?
    if [ $retVal -ne 0 ]; then
        echo "!!!!!! Error with JS integration test !!!!!!"
        exit 1
    fi

    echo "Success with integration test"
    exit 0
fi
