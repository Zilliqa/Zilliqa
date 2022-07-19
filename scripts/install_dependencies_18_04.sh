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
# This script is dedicated for CI use
#

set -e

sudo apt-get update

sudo apt-get install git libboost-system-dev libboost-filesystem-dev libboost-test-dev \
    libssl-dev libleveldb-dev libjsoncpp-dev libsnappy-dev libmicrohttpd-dev \
    libjsonrpccpp-dev build-essential pkg-config libevent-dev libminiupnpc-dev \
    libcurl4-openssl-dev libboost-program-options-dev libboost-python-dev python3-dev \
    python3-setuptools python3-pip gawk

ls -lath ./
