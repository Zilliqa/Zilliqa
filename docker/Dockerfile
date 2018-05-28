# escape=\
FROM ubuntu:16.04

#--------------------------------------------------------------------
# PACKAGES
#--------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libboost-filesystem-dev \
    libboost-system-dev \
    libboost-test-dev \
    libjsoncpp-dev \
    libjsonrpccpp-dev \
    libleveldb-dev \
    libmicrohttpd-dev \
    libsnappy-dev \
    libssl-dev \
    libjsonrpccpp-dev \
    libevent-dev \
    python python-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip install netaddr

#--------------------------------------------------------------------
# BUILDING
#--------------------------------------------------------------------
ARG BRANCH=master
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local

# FIXME: see issue https://github.com/Zilliqa/Zilliqa/issues/240
RUN git clone --recursive -b ${BRANCH} --depth=1 ${REPO} ${SOURCE_DIR}; \
    mkdir $BUILD_DIR && cd $BUILD_DIR; \
    cmake -DIS_LOOKUP_NODE=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR}; \
    make -j2 && make install; \
    cd .. && rm -rf ${BUILD_DIR}; \
    mv ${INSTALL_DIR}/bin/zilliqa ${INSTALL_DIR}/bin/zilliqa_lookup; \
    mkdir $BUILD_DIR && cd $BUILD_DIR; \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR}; \
    make -j2 && make install; \
    cd ${INSTALL_DIR} && rm -rf ${BUILD_DIR}

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
