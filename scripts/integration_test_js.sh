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

    # install dependencies
    apt update
    apt -y install curl dirmngr apt-transport-https lsb-release ca-certificates
    curl -sL https://deb.nodesource.com/setup_14.x | bash -
    apt -y install nodejs
    node --version
    pwd

    pkill -9 isolatedServer
    pkill -9 evm-ds

    # remove persistence
    rm -rf persistence/*

    # maybe it's there
    rm -rf ./build/bin/persistence

    # Just to check evm-ds has been built
    if [[ -d /home/jenkins ]]; then
        #ls /home/jenkins/agent/workspace/ZilliqaCIJenkinsfile_PR-*/evm-ds/target/release/evm-ds

        ls -l /home/jenkins/agent/workspace/

        # For convenience move the required files to tmp directory
        cp /home/jenkins/agent/workspace/*/evm-ds/target/release/evm-ds /tmp || exit 1
        cp /home/jenkins/agent/workspace/*/evm-ds/log4rs.yml /tmp

        # Modify constants.xml for use by isolated server
        cp constants.xml constants_backup.xml
        sed -i 's/.LOOKUP_NODE_MODE.false/<LOOKUP_NODE_MODE>true/g' constants.xml
        sed -i 's/.ENABLE_EVM>.*/<ENABLE_EVM>true<\/ENABLE_EVM>/g' constants.xml
        sed -i 's/.EVM_SERVER_BINARY.*/<EVM_SERVER_BINARY>\/tmp\/evm-ds<\/EVM_SERVER_BINARY>/g' constants.xml
        sed -i 's/.EVM_LOG_CONFIG.*/<EVM_LOG_CONFIG>\/tmp\/log4rs.yml<\/EVM_LOG_CONFIG>/g' constants.xml
    fi

    echo "Starting isolated server..."
    ./build/bin/isolatedServer -f isolated-server-accounts.json -u 999 -t 3000 &

    sleep 10

    cd tests/EvmAcceptanceTests/
    npm install
    DEBUG=true MOCHA_TIMEOUT=300000 PARALLEL=true npx hardhat test

    retVal=$?
    if [ $retVal -ne 0 ]; then
        echo "!!!!!! Error with JS integration test !!!!!!"
        exit 1
    fi

    echo "Success with integration test"
    exit 0
fi
