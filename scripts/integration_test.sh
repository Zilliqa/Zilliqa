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

# Need to build evm...
git clone https://github.com/Zilliqa/evm-ds.git

cd evm-ds
cargo --version
echo "building"
sudo snap install protobuf --classic
cargo build --verbose --release --package evm-ds

cd -

sudo mkdir -p /usr/local/etc/
cp log4rs.yml /usr/local/etc/
ls -lath /usr/local/etc/

# Just to check evm-ds has been built
ls /home/travis/build/Zilliqa/Zilliqa/evm-ds/target/release/evm-ds

# Modify constants.xml for use by isolated server
cp constants.xml constants_backup.xml
sed -i 's/.LOOKUP_NODE_MODE.false/<LOOKUP_NODE_MODE>true/g' constants.xml
sed -i 's/.ENABLE_EVM>.*/<ENABLE_EVM>true<\/ENABLE_EVM>/g' constants.xml
sed -i 's/.EVM_SERVER_BINARY.*/<EVM_SERVER_BINARY>\/home\/travis\/build\/Zilliqa\/Zilliqa\/evm-ds\/target\/release\/evm-ds<\/EVM_SERVER_BINARY>/g' constants.xml

cat constants.xml
echo ""
echo ""

echo "Starting isolated server"
./build/bin/isolatedServer -f isolated-server-accounts.json -u 999 &

sleep 15

echo "Starting python test"
sudo apt-get install python3-pip python3-setuptools python3-pip python3-dev python-setuptools-doc python3-wheel 2>&1 > /dev/null

python3 -m pip install cython 2>&1 > /dev/null

python3 --version
python3 -m pip install -r ./tests/PythonEthApi/requirements.txt || exit 1
python3 ./tests/PythonEthApi/test_api.py --api http://localhost:5555 > out.txt

retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Error with integration test"
    cat out.txt
    exit 1
fi

# Make constants.xml as it was
mv constants_backup.xml constants.xml

echo "Success with integration test"
exit 0
