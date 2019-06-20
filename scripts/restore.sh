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


path_to_binary="/usr/local/bin/restore"
prev_persistence_path=$1
new_persistence_path=$2
epoch_num=$3


function change_download_db()
{
        sed -i '/Exclude_txnBodies =/c\Exclude_txnBodies = False' ./downloadIncrDB.py
        sed -i '/Exclude_microBlocks =/c\Exclude_microBlocks = False' ./downloadIncrDB.py
}

function clean()
{
        rm -rf $new_persistence_path
        mkdir $new_persistence_path
        cd $new_persistence_path
}

function copy_essentials()
{
        cp ../dsnodes.xml .
        cp ../constants.xml .
        cp -r $prev_persistence_path/persistence .
        cp ../downloadIncrDB.py .
}

if [ ! -d $prev_persistence_path/persistence ]; then
    echo "Could not find persistence folder $prev_persistence_path!"
    exit 1
fi

clean
copy_essentials
change_download_db
./downloadIncrDB.py
#run the restore command
$path_to_binary $epoch_num
