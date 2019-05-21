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


path_to_binary="/usr/local/restore"
epoch_num=$1
directory_name="restore"


function change_download_db()
{
	sed -i '/Exclude_txnBodies /c\Exclude_txnBodies = True' ./downloadIncrDB.py
	sed -i '/Exclude_microBlocks /c\Exclude_microBlocks = True' ./downloadIncrDB.py
}


function clean()
{
	rm $directory_name
	mkdir $directory_name
	cd $directory_name
}

function copy_essentials()
{
	cp ../dsnodes.xml .
	cp ../constants.xml .
	cp -r persistence/ .
	cp ../downloadIncrDB.py .
}

clean
copy_essentials
change_download_db

#run the restore command
$path_to_binary $epoch_num

