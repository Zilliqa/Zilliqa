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
# The script is dedicated for CI use, do not run it on your machine
#

set -e

function install_libmongoc() {
# install libmongoc-1.13.0
# see http://mongoc.org/libmongoc/current/installing.html
curl -OL https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz
tar xzf mongo-c-driver-1.13.0.tar.gz
cd mongo-c-driver-1.13.0
mkdir cmake-build
cd cmake-build
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
make
make install
}

function install_libongocxx() {
# install libmongocxx-3.3.1
# see https://mongodb.github.io/mongo-cxx-driver/mongocxx-v3/installation/
curl -OL https://github.com/mongodb/mongo-cxx-driver/archive/r3.3.1.tar.gz
tar -xzf r3.3.1.tar.gz
cd mongo-cxx-driver-r3.3.1/build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
make EP_mnmlstc_core
make -j -l4
make install
}

function install_openssl() {
# install openssl-1.1.1
# only 1.1.x has the necessary BN_generate_dsa_nonce function in bn.h
curl -sL https://www.openssl.org/source/openssl-1.1.1.tar.gz
tar -xzf openssl-1.1.1.tar.gz
cd openssl-1.1.1
./config -Wl,--enable-new-dtags,-rpath,'$(LIBRPATH)' > /dev/null 2>&1
make
make install
}

# presently a docker version ubuntu 16.04 is used
function on_sudoless_ubuntu() {

apt-get -qq update

# install build dependencies
apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
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

# install openssl-1.1.1
install_openssl

# install development dependencies
apt-get install -y \
    ccache \
    clang-format-5.0 \
    clang-tidy-5.0 \
    clang-5.0 \
    lcov \
    curl \
    libxml2-utils \
    python-pip \
    git

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
        on_sudoless_ubuntu
        # mongodb drivers are already installed in the image used for Linux build
        ;;
    'Darwin')
        echo "Installing dependencies on OSX ..."
        on_osx
        ;;
    *)
        echo "Error: Unknown OS, no dependencies installed"
        ;;
esac

