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
    libprotobuf-dev \
    protobuf-compiler \
    libcurl4-openssl-dev \
    python python-pip \
    curl \
    libsasl2-dev \
    && rm -rf /var/lib/apt/lists/* \
    && pip install netaddr

ARG MONGOC_DIR=/mongo-c-driver
ARG MONGOC_BUILD_DIR=${MONGOC_DIR}/build

ARG MONGOCXX_DIR=/mongo-cxx-driver
ARG MONGOCXX_BUILD_DIR=${MONGCXX_DIR}/build

RUN curl -OL https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz && \
    mkdir -p ${MONGOC_DIR} && tar xzf mongo-c-driver-1.13.0.tar.gz -C ${MONGOC_DIR} && \
    cmake -H${MONGOC_DIR} -B${MONGOC_BUILD_DIR} -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
    cmake --build ${MONGOC_BUILD_DIR} -- -j${JOBS} && \
    cmake --build ${MONGOC_BUILD_DIR} --target install && \
    curl -OL https://github.com/mongodb/mongo-cxx-driver/archive/r3.3.1.tar.gz && \
    mkdir -p ${MONGCXX_DIR} && tar -xzf r3.3.1.tar.gz -C ${MONGCXX_DIR} && \
    cmake -H${MONGCXX_DIR} -B${MONGOCXX_BUILD_DIR} -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build ${MONGOCXX_BUILD_DIR} --target EP_mnmlstc_core && \
    cmake --build ${MONGOCXX_BUILD_DIR} --target -j ${JOBS} && \
    cmake --build ${MONGOCXX_BUILD_DIR} --target install

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
    cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR} && \
    make -j${JOBS} && make install && \
    cd ${INSTALL_DIR} && rm -rf ${BUILD_DIR}

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

ENTRYPOINT ["/bin/bash"]
