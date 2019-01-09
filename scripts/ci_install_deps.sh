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
# The script is dedicated for CI use, do not run it on your machine
#

set -e

# depends directory, assume script executed from root
DEPENDS_DIR=./src/depends

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
# install openssl-1.1.1 into src/depends
# only 1.1.x has the necessary BN_generate_dsa_nonce function in bn.h
cd ${DEPENDS_DIR}
curl -OL https://www.openssl.org/source/openssl-1.1.1.tar.gz
tar -xzf openssl-1.1.1.tar.gz
cd openssl-1.1.1
./config -Wl,--enable-new-dtags,-rpath,'$(LIBRPATH)' > /dev/null
make > /dev/null
make install > /dev/null
cd ..
rm -rf openssl-1.1.1 openssl-1.1.1.tar.gz
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

