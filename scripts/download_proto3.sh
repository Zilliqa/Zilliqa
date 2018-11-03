#!/bin/sh
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
