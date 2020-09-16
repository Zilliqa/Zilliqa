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

ARG BASE=zilliqa/scilla:v0.9.0
FROM ${BASE} AS scilla
# run a copy -L to unfold the symlinkes, and strip all exes
RUN mkdir -p /scilla/0/bin2/ && cp -L /scilla/0/bin/* /scilla/0/bin2/ && strip /scilla/0/bin2/*

# start from a new ubuntu environment as builder for zilliqa, make sure the deps is consistent with those in the runner image
FROM ubuntu:18.04 AS builder
# Format guideline: one package per line and keep them alphabetically sorted
RUN apt-get update \
    && apt-get install -y software-properties-common \
    && apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    build-essential \
    ca-certificates \
    cmake \
    # curl is not a build dependency
    curl \
    git \
    golang \
    # rysnc bydefault gets installed with opam package of scilla.Better to explicitly
    # mention again as installation candidate
    rsync \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-test-dev \
    libboost-python-dev \
    libcurl4-openssl-dev \
    libevent-dev \
    libjsoncpp-dev \
    libjsonrpccpp-dev \
    libleveldb-dev \
    libmicrohttpd-dev \
    libminiupnpc-dev \
    libsnappy-dev \
    libssl-dev \
    libtool \
    ocl-icd-opencl-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    python3-setuptools \
    libsecp256k1-dev \
    && rm -rf /var/lib/apt/lists/*

# Manually input tag or commit, can be overwritten by docker build-args
ARG COMMIT_OR_TAG=v7.0.0-alpha.6
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local
ARG BUILD_TYPE=RelWithDebInfo
ARG EXTRA_CMAKE_ARGS=
ARG MONGO_INSTALL_DIR=${BUILD_DIR}/mongo

RUN git clone ${REPO} ${SOURCE_DIR} \
    && git -C ${SOURCE_DIR} checkout ${COMMIT_OR_TAG} \
    && cmake -H${SOURCE_DIR} -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${EXTRA_CMAKE_ARGS} \
    && cmake --build ${BUILD_DIR} -- -j$(nproc) \
    && cmake --build ${BUILD_DIR} --target install \
    && echo "built files:" && ls -lh ${BUILD_DIR} && echo "installed libs:" && ls -lh ${INSTALL_DIR}/lib \
    && echo "mongo files:" && ls -lh ${MONGO_INSTALL_DIR}/lib \
    # strip all exes
    && strip /usr/local/bin/grepperf \
       /usr/local/bin/zilliqad \
       /usr/local/bin/genkeypair \
       /usr/local/bin/signmultisig \
       /usr/local/bin/verifymultisig \
       /usr/local/bin/getpub \
       /usr/local/bin/getaddr \
       /usr/local/bin/genaccounts \
       /usr/local/bin/sendcmd \
       /usr/local/bin/gentxn \
       /usr/local/bin/restore \
       /usr/local/bin/gensigninitialds \
       /usr/local/bin/validateDB \
       /usr/local/bin/genTxnBodiesFromS3 \
       /usr/local/bin/getnetworkhistory \
       /usr/local/bin/isolatedServer \
       /usr/local/bin/getrewardhistory \
    #    /usr/local/bin/zilliqa \
       /usr/local/bin/data_migrate \
       /usr/local/lib/libSchnorr.so \
       /usr/local/lib/libethash.so \
       /usr/local/lib/libNAT.so \
       /usr/local/lib/libCommon.so \
       /usr/local/lib/libTrie.so

# start from a new ubuntu as the runner image
FROM ubuntu:18.04

# install all necessary libraries
RUN apt-get update \
    && apt-get install -y software-properties-common \
    && apt-get update && apt-get install -y --no-install-recommends \
    # libs
    ca-certificates \
    libboost-filesystem-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-test-dev \
    libboost-python-dev \
    libcurl4-openssl-dev \
    libevent-dev \
    libjsoncpp-dev \
    libjsonrpccpp-dev \
    libleveldb-dev \
    libmicrohttpd-dev \
    libminiupnpc-dev \
    libsnappy-dev \
    libssl-dev \
    libtool \
    ocl-icd-opencl-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    python3-setuptools \
    libsecp256k1-dev \
    # tools
    curl \
    dnsutils \
    gdb \
    git \
    less \
    logrotate \
    net-tools \
    rsync \
    rsyslog \
    trickle \
    vim \
    && rm -rf /var/lib/apt/lists/*

# install all necessary libraries for python
COPY requirements3.txt ./
RUN pip3 install wheel \
    && pip3 install --no-cache-dir -r requirements3.txt \
    && update-alternatives --install /usr/bin/python python /usr/bin/python3 10 # set python3 as default instead python2

# make dirs fro scilla and zilliqa
RUN mkdir -p \
    /scilla/0/bin /scilla/0/src/stdlib \
    /zilliqa/scripts

ARG INSTALL_DIR=/usr/local
ARG MONGO_INSTALL_DIR=/build/mongo

# pour in scilla binaries
COPY --from=scilla  /scilla/0/bin2            /scilla/0/bin
# pour in scilla conntract stdlibs
COPY --from=scilla  /scilla/0/src/stdlib     /scilla/0/src/stdlib
# pour in zilliqa scripts
COPY --from=builder /zilliqa/scripts         /zilliqa/scripts
# pour in zilliqa binaries and dynnamic libs
COPY --from=builder ${INSTALL_DIR}/bin/*     ${INSTALL_DIR}/bin/
COPY --from=builder ${INSTALL_DIR}/lib/*.so* ${INSTALL_DIR}/lib/
COPY --from=builder ${MONGO_INSTALL_DIR}/lib/*.so* ${INSTALL_DIR}/lib/ 

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib:${MONGO_INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
