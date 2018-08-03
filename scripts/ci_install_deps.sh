#!/bin/bash
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
    ocl-icd-opencl-dev

# install development dependencies
apt-get install -y \
    ccache \
    clang-format-5.0 \
    clang-tidy-5.0 \
    clang-5.0 \
    lcov \
    curl
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

