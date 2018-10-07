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
#
# The script is dedicated for CI use
#

set -e

# presently a docker version ubuntu 16.04 is used
function on_sudoless_ubuntu() {

apt-get -qq update

# install build dependencies
apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
    libssl-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-test-dev \
    libleveldb-dev \
    libsnappy-dev \
    libjsoncpp-dev \
    libmicrohttpd-dev \
    libjsonrpccpp-dev \
    libminiupnpc-dev \
    libevent-dev \
    libprotobuf-dev \
    libcurl4-openssl-dev \
    protobuf-compiler

# install development dependencies
apt-get install -y \
    ccache \
    clang-format-5.0 \
    clang-tidy-5.0 \
    clang-5.0 \
    lcov \
    curl \
    libxml2-utils \
    python-pip

pip install pyyaml
}

function on_osx() {

# travis will keep brew updated
export HOMEBREW_NO_AUTO_UPDATE=1

# install build deps
brew install \
    pkg-config \
    jsoncpp \
    leveldb \
    libjson-rpc-cpp \
    miniupnpc \
    libevent \
    protobuf

# install developement deps
brew install \
    ccache \
    llvm@5
}

if [ "${TRAVIS}" != "true" -a "${CI}" != "true" ]
then
    echo "No CI environment detected, continue [y/N]?:"
    read force
    case $force in
        'y')
            ;;
        *)
            echo "Dependency installation stopped"
            exit 1
            ;;
    esac
fi

os=$(uname)

# only need to distinguish linux and osx
case $os in
    'Linux')
        echo "Installing dependencies on Linux ..."
        on_sudoless_ubuntu || echo "Hint: Try re-run with sudo right, if failed"
        ;;
    'Darwin')
        echo "Installing dependencies on OSX ..."
        on_osx
        ;;
    *)
        echo "Error: Unknown OS, no dependencies installed"
        ;;
esac

