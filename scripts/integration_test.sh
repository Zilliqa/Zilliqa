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
    echo "You are running the script locally. The requirements for this is only that the isolated server "
    echo "is running on port 5555"
    echo "You must also have installed the requirements.txt in tests/PythonEthApi/"
    echo ""

    python3 tests/PythonEthApi/test_api.py --api http://localhost:5555
else
    echo "The CI is running this script."
    # Install dependencies silently on the CI server
    echo "Installing protobuf..."
    apt install -y protobuf-compiler 2>&1 > /dev/null
    echo "Installing python3"
    apt-get install -y python3-pip python3-setuptools python3-pip python3-dev python-setuptools-doc python3-wheel 2>&1 > /dev/null
    python3 -m pip install cython 2>&1 > /dev/null
    echo "Installing requirements"
    python3 -m pip install -r ./tests/PythonEthApi/requirements.txt 2>&1 > /dev/null

    cd evm-ds
    cargo --version
    echo "building evm ds"

    cargo build --release --package evm-ds -q

    echo "built evm ds"

    cd -

    # Just to check evm-ds has been built
    if [[ -d /home/travis ]]; then
        ls /home/travis/build/Zilliqa/Zilliqa/evm-ds/target/release/evm-ds

        # Modify constants.xml for use by isolated server
        cp constants.xml constants_backup.xml
        sed -i 's/.LOOKUP_NODE_MODE.false/<LOOKUP_NODE_MODE>true/g' constants.xml
        sed -i 's/.ENABLE_EVM>.*/<ENABLE_EVM>true<\/ENABLE_EVM>/g' constants.xml
        sed -i 's/.EVM_SERVER_BINARY.*/<EVM_SERVER_BINARY>\/home\/travis\/build\/Zilliqa\/Zilliqa\/evm-ds\/target\/release\/evm-ds<\/EVM_SERVER_BINARY>/g' constants.xml
        sed -i 's/.EVM_LOG_CONFIG.*/<EVM_LOG_CONFIG>\/home\/travis\/build\/Zilliqa\/Zilliqa\/evm-ds\/log4rs.yml<\/EVM_LOG_CONFIG>/g' constants.xml
    fi

    if [[ -d /home/jenkins ]]; then
        pwd
        ls /home/jenkins/agent/workspace/ZilliqaCIJenkinsfile_PR-*/evm-ds/target/release/evm-ds

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

    echo "Starting isolated server"
    ./build/bin/isolatedServer -f isolated-server-accounts.json -u 999 -t 3000 &

    sleep 15

    # Cat the python test so it doesn't interleave with the isolated server.
    echo "Starting python test"
    python3 ./tests/PythonEthApi/test_api.py --api http://localhost:5555 2>&1 > out.txt

    retVal=$?
    if [ $retVal -ne 0 ]; then
        echo "!!!!!! Error with integration test !!!!!!"
        cat out.txt
        exit 1
    fi

    cat out.txt

    # Make constants.xml as it was
    mv constants_backup.xml constants.xml

    echo "Success with integration test"
    exit 0
fi
