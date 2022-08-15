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

# Install dependencies silently
sudo snap install protobuf --classic 2>&1 > /dev/null
sudo apt-get install python3-pip python3-setuptools python3-pip python3-dev python-setuptools-doc python3-wheel 2>&1 > /dev/null
python3 -m pip install cython 2>&1 > /dev/null
python3 -m pip install -r ./tests/PythonEthApi/requirements.txt 2>&1 > /dev/null

# Need to build evm...
git clone https://github.com/Zilliqa/evm-ds.git

cd evm-ds
cargo --version
echo "building evm ds"

cargo build --verbose --release --package evm-ds 2>&1 > /dev/null

cd -

# Just to check evm-ds has been built
ls /home/travis/build/Zilliqa/Zilliqa/evm-ds/target/release/evm-ds

# Modify constants.xml for use by isolated server
cp constants.xml constants_backup.xml
sed -i 's/.LOOKUP_NODE_MODE.false/<LOOKUP_NODE_MODE>true/g' constants.xml
sed -i 's/.ENABLE_EVM>.*/<ENABLE_EVM>true<\/ENABLE_EVM>/g' constants.xml
sed -i 's/.EVM_SERVER_BINARY.*/<EVM_SERVER_BINARY>\/home\/travis\/build\/Zilliqa\/Zilliqa\/evm-ds\/target\/release\/evm-ds<\/EVM_SERVER_BINARY>/g' constants.xml
sed -i 's/.EVM_LOG_CONFIG.*/<EVM_LOG_CONFIG>\/home\/travis\/build\/Zilliqa\/Zilliqa\/evm-ds\/log4rs.yml<\/EVM_LOG_CONFIG>/g' constants.xml

echo "Starting isolated server"
./build/bin/isolatedServer -f isolated-server-accounts.json -u 999 &

sleep 15

# Cat the python test so it doesn't interleave with the isolated server.
echo "Starting python test"
python3 ./tests/PythonEthApi/test_api.py --api http://localhost:5555 2>&1 > out.txt

retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Error with integration test"
    exit 1
fi

cat out.txt

# Make constants.xml as it was
mv constants_backup.xml constants.xml

echo "Success with integration test"
exit 0
