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

# [MUST BE FILLED IN] User configuration settings
aws_access_key_id=""
aws_secret_access_key=""


# Validate input argument
if [ "$#" -ne 1 ]; then
    echo -e "\n\n\033[0;31mUsage: source ./scripts/validateBackupDB.sh backupDBName\033[0m\n"
    return 0
fi

if [ "$aws_access_key_id" = "" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Please enter your own AWS_ACCESS_KEY_ID in validateBackupDB.sh!\033[0m\n"
    return 0
fi

if [ "$aws_secret_access_key" = "" ]; then
    echo -e "\n\n\033[0;31m*ERROR* Please enter your own AWS_SECRET_ACCESS_KEY in validateBackupDB.sh!\033[0m\n"
    return 0
fi


# Download persistence from Amazon S3 database
backupDBName="$1"
export AWS_ACCESS_KEY_ID=${aws_access_key_id}
export AWS_SECRET_ACCESS_KEY=${aws_secret_access_key}
pip install awscli
aws s3 cp s3://zilliqa-persistence/${backupDBName}.tar.gz ${backupDBName}.tar.gz
rm -rf persistence
tar xzvf ${backupDBName}.tar.gz
echo -e "\n\033[0;32mDownload ${backupDBName}.tar.gz from Amazon S3 database successfully.\033[0m\n"


# Configure testing environment
sed -i '/<GUARD_MODE>/c\        <GUARD_MODE>true</GUARD_MODE>' constants_local.xml
sed -i '/<NUM_DS_ELECTION>/c\        <NUM_DS_ELECTION>1</NUM_DS_ELECTION>' constants_local.xml
sed -i '/<NUM_FINAL_BLOCK_PER_POW>/c\        <NUM_FINAL_BLOCK_PER_POW>100</NUM_FINAL_BLOCK_PER_POW>' constants_local.xml
sed -i '/<REJOIN_NODE_NOT_IN_NETWORK>/c\        <REJOIN_NODE_NOT_IN_NETWORK>false</REJOIN_NODE_NOT_IN_NETWORK>' constants_local.xml
./build.sh
cd build
./tests/Node/pre_run.sh
cd -
rm -rf build/local_run/node_0001/persistence
cp -rf persistence build/local_run/node_0001/
echo -e "\n\n\033[0;32mConfigure testing environment successfully.\033[0m\n"


# Start testing
cd build
./tests/Node/test_node_validateBackupDB.py


# Verify testing result
if grep -q "RetrieveHistory Success" local_run/node_0001/zilliqa-00001-log.txt;
then
    echo -e "\n\n\033[0;32mValidation of persistence ${backupDBName} PASS!\033[0m\n"
else
    echo -e "\n\n\033[0;31mValidation of persistence ${backupDBName} FAIL!\033[0m\n"
fi


# Clean up
pkill zilliqa
cd -
