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

ARG BASE=zilliqa/scilla:v0.8.0
FROM ${BASE}

COPY requirements3.txt ./


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
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install -r requirements3.txt

# set python3 as default instead python2
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 10

# Manually input tag or commit, can be overwritten by docker build-args
ARG COMMIT_OR_TAG=v6.3.0-alpha.0
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local
ARG BUILD_TYPE=RelWithDebInfo
ARG EXTRA_CMAKE_ARGS=

RUN git clone ${REPO} ${SOURCE_DIR} \
    && git -C ${SOURCE_DIR} checkout ${COMMIT_OR_TAG} \
    && cmake -H${SOURCE_DIR} -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${EXTRA_CMAKE_ARGS} \
    && cmake --build ${BUILD_DIR} -- -j$(nproc) \
    && cmake --build ${BUILD_DIR} --target install \
    && rm -rf ${BUILD_DIR}

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
