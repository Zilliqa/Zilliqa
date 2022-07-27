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

find ./ -name isolatedServer

# Modify constants.xml for use by isolated server
cp constants.xml constants_backup.xml
sed -i 's/.ENABLE_SC.true/<ENABLE_SC>false/g' constants.xml

echo "Starting isolated server"
./build/bin/isolatedServer -f isolated-server-accounts.json -u 999 &

sleep 15

echo "Starting python test"
python3 ./tests/PythonEthApi/test_api.py --api http://localhost:5555 > out.txt

# Make constants.xml as it was
mv constants_backup.xml constants.xml

retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Error with integration test"
    cat out.txt
    exit 1
fi

echo "Success with integration test"
exit 0
