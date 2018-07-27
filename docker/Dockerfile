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
    libminiupnpc-dev \
    ocl-icd-opencl-dev \
    python python-pip \
    && rm -rf /var/lib/apt/lists/* \
    && pip install netaddr

#--------------------------------------------------------------------
# BUILDING
#--------------------------------------------------------------------
# don't do check-out if COMMIT is empty
ARG COMMIT=
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local
ARG BUILD_TYPE=RelWithDebInfo
ARG JOBS=2

# FIXME: see issue https://github.com/Zilliqa/Zilliqa/issues/240
RUN git clone --recursive ${REPO} ${SOURCE_DIR} && \
    git -C ${SOURCE_DIR} checkout ${COMMIT} && \
    mkdir $BUILD_DIR && cd $BUILD_DIR && \
    cmake -DIS_LOOKUP_NODE=1 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR} && \
    make -j${JOBS} && make install && \
    cd .. && rm -rf ${BUILD_DIR} && \
    mv ${INSTALL_DIR}/bin/zilliqa ${INSTALL_DIR}/bin/zilliqa_lookup && \
    mkdir $BUILD_DIR && cd $BUILD_DIR && \
    cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR} && \
    make -j${JOBS} && make install && \
    cd ${INSTALL_DIR} && rm -rf ${BUILD_DIR}

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
