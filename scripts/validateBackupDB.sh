#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your
# participation in testing Zilliqa. You may view, compile and run the code for
# that purpose and pursuant to the protocols and algorithms that are programmed
# into, and intended by, the code. You may not do anything else with the code
# without express permission from Zilliqa Research Pte. Ltd., including
# modifying or publishing the code (or any part of it), and developing or
# forming another public or private blockchain network. This source code is
# provided 'as is' and no warranties are given as to title or non-infringement,
# merchantability or fitness for purpose and, to the extent permitted by law,
# all liability for your use of the code is disclaimed. Some programs in this
# code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
# are governed by GPLv3.0 are those programs that are located in the folders
# src/depends and tests/depends and which include a reference to GPLv3 in their
# program files.

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
