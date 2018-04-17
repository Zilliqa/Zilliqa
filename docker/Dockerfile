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
    && rm -rf /var/lib/apt/lists/*

#--------------------------------------------------------------------
# BUILDING
#--------------------------------------------------------------------
ARG BRANCH=feature/docker
ARG REPO=https://github.com/Zilliqa/Zilliqa.git

ENV Z_INSTALL_PREFIX=/zilliqa
ENV LD_LIBRARY_PATH=${Z_INSTALL_PREFIX}/lib

RUN git clone -b ${BRANCH} --depth=1 ${REPO} && \
    cd Zilliqa && mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j4 && mv bin .. && cd .. && rm -rf build

ENTRYPOINT ["/bin/bash"]
