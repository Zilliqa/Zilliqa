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
FROM zilliqa/scilla

# Format guideline: one package per line and keep them alphabetically sorted
RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    build-essential \
    ca-certificates \
    cmake \
    # curl is not a build dependency
    curl \
    git \
    golang \
    libboost-filesystem-dev \
    libboost-system-dev \
    libboost-test-dev \
    libcurl4-openssl-dev \
    libevent-dev \
    libjsoncpp-dev \
    libjsonrpccpp-dev \
    libleveldb-dev \
    libmicrohttpd-dev \
    libminiupnpc-dev \
    libprotobuf-dev \
    libsnappy-dev \
    libssl-dev \
    libtool \
    ocl-icd-opencl-dev \
    pkg-config \
    protobuf-compiler \
    python \
    python-pip \
    && rm -rf /var/lib/apt/lists/*

RUN curl -sL https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz | tar -zx -C . \
    && cmake -Hmongo-c-driver-1.13.0 -Bmongo-c-build -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build mongo-c-build -- -j$(nproc) \
    && cmake --build mongo-c-build --target install \
    && rm -rf mongo-c-* \
    && curl -sL https://github.com/mongodb/mongo-cxx-driver/archive/r3.3.1.tar.gz | tar -zx -C . \
    && cmake -Hmongo-cxx-driver-r3.3.1 -Bmongo-cxx-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build mongo-cxx-build --target EP_mnmlstc_core \
    && cmake --build mongo-cxx-build -- -j$(nproc) \
    && cmake --build mongo-cxx-build --target install \
    && rm -rf mongo-cxx-*

ARG ZILLIQA_VERSION=v3.2.0
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local
ARG BUILD_TYPE=RelWithDebInfo
ARG EXTRA_CMAKE_ARGS=

RUN git clone -b ${ZILLIQA_VERSION} --depth 1 ${REPO} ${SOURCE_DIR} \
    && cmake -H${SOURCE_DIR} -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${EXTRA_CMAKE_ARGS} \
    && cmake --build ${BUILD_DIR} -- -j$(nproc) \
    && cmake --build ${BUILD_DIR} --target install \
    && rm -rf ${BUILD_DIR}

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
