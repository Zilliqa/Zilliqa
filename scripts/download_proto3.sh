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

PROTOC_REQUIRED_VERSION="3.6.1"
PROTOC_DOWNLOAD_URL="https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_REQUIRED_VERSION}/protobuf-cpp-${PROTOC_REQUIRED_VERSION}.tar.gz"

# detect ubuntu 16.04 lts version
OS_VERSION=`lsb_release -rs`
if [ -z "$OS_VERSION" ] || [ "$OS_VERSION" != "16.04" ]; then
  echo "Not Ubuntu 16.04 LTS version"
  exit 1
fi

# detect current protoc version
PROTOC_BIN=`which protoc`
if [ ! -z "$PROTOC_BIN" ]; then
  PROTOC_VERSION=`${PROTOC_BIN} --version | cut -d ' ' -f 2`

  # check if required version already exists.
  if [ "$PROTOC_VERSION" = "$PROTOC_REQUIRED_VERSION" ]; then
    echo "Protoc ${PROTOC_REQUIRED_VERSION} already installed. Nothing else to do"
    exit 0
  else
    echo "Required protoc version: ${PROTOC_REQUIRED_VERSION}"
    echo "Existing protoc version: ${PROTOC_VERSION}"
    echo "Please delete existing version..."
    exit 1
  fi
fi

# download proto3
mkdir -p /tmp/protoc
wget -qO- ${PROTOC_DOWNLOAD_URL} | tar xvz -C /tmp/protoc
cd /tmp/protoc/protobuf-${PROTOC_REQUIRED_VERSION}
./configure && make && sudo make install && sudo ldconfig

sudo ln -s /usr/local/bin/protoc /usr/bin/
sudo ln -s /usr/local/lib/libprotobuf.so /usr/lib/x86_64-linux-gnu/
