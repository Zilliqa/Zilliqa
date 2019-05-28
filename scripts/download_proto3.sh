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

PROTOC_MIN_VERSION="3.1.0"
PROTOBUF3_PPA_SRC="maarten-fonville/protobuf"
PROTOC_DOWNLOAD_URL="https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOC_MIN_VERSION}/protobuf-cpp-${PROTOC_MIN_VERSION}.tar.gz"

# detect ubuntu 16.04 lts version
OS_VERSION=`lsb_release -rs`
if [ -z "$OS_VERSION" ] || [ "$OS_VERSION" != "16.04" ]; then
  echo "Not Ubuntu 16.04 LTS version"
  exit 1
fi

# detect current protoc version
PROTOC_BIN=`which protoc`
if [ ! -z "$PROTOC_BIN" ]; then
  PROTOC_CURRENT_VERSION=`${PROTOC_BIN} --version | cut -d ' ' -f 2`
  PROTOC_CURRENT_MAJOR_VERSION=`echo ${PROTOC_CURRENT_VERSION} | cut -d '.' -f 1`
  PROTOC_CURRENT_MINOR_VERSION=`echo ${PROTOC_CURRENT_VERSION} | cut -d '.' -f 2`

  PROTOC_MIN_MAJOR_VERSION=`echo ${PROTOC_MIN_VERSION} | cut -d '.' -f 1`
  PROTOC_MIN_MINOR_VERSION=`echo ${PROTOC_MIN_VERSION} | cut -d '.' -f 2`

  # check if required version already exists.
  if [ "$PROTOC_CURRENT_MAJOR_VERSION" -ge "$PROTOC_MIN_MAJOR_VERSION" ] && [ "$PROTOC_CURRENT_MINOR_VERSION" -ge "$PROTOC_MIN_MINOR_VERSION" ] ; then
    echo "Protoc ${PROTOC_MIN_VERSION} already installed. Nothing else to do"
    exit 0
  else
    echo "Required protoc version: ${PROTOC_MIN_VERSION}"
    echo "Existing protoc version: ${PROTOC_CURRENT_VERSION}"
    echo "Please delete existing version..."
    exit 1
  fi
fi

echo "Add ppa:${PROTOBUF3_PPA_SRC}"
add-apt-repository --yes ppa:${PROTOBUF3_PPA_SRC}
if [[ $? > 0 ]]; then
  echo "Cannot add ppa:${PROTOBUF3_PPA_SRC}"
else
  echo "apt-get updating"
  apt-get update
  if [[ $? > 0 ]]; then
    echo "Cannot update apt-get after adding ppa:${PROTOBUF3_PPA_SRC}"
  else
    apt-get install --yes libprotobuf-dev protobuf-compiler
    if [[ $? > 0 ]]; then
      echo "Cannot get expected protbuf from ppa:${PROTOBUF3_PPA_SRC}"
    else
      echo "protobuf version:"
      exit 0
    fi
  fi
fi

wget -q https://raw.githubusercontent.com/linux-on-ibm-z/scripts/master/Protobuf/3.7.1/build_protobuf.sh
bash build_protobuf.sh -y

if ! [[ $? > 0 ]]; then
  rm build_protobuf.sh
  exit 0
fi

# download proto3
mkdir -p /tmp/protoc3
curl -OL ${PROTOC_DOWNLOAD_URL}; tar -xvzf protobuf-cpp-${PROTOC_MIN_VERSION}.tar.gz -C /tmp/protoc3
cd /tmp/protoc3/protobuf-${PROTOC_MIN_VERSION}
./configure && make && make install && ldconfig

ln -s /usr/local/bin/protoc /usr/bin/
ln -s /usr/local/lib/libprotobuf.so /usr/lib/x86_64-linux-gnu/