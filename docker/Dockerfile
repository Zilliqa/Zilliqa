# escape=\
FROM ubuntu:16.04

#--------------------------------------------------------------------
# PACKAGES - UPDATE DATABASE
#--------------------------------------------------------------------
RUN apt-get -qq update

#--------------------------------------------------------------------
# PACKAGES - BUILDING
#--------------------------------------------------------------------
RUN apt-get install -y pkg-config
RUN apt-get install -y build-essential
RUN apt-get install -y cmake
RUN apt-get install -y git

#--------------------------------------------------------------------
# PACKAGES - SOURCE DEPENDENCIES
#--------------------------------------------------------------------
RUN apt-get install -y libboost-all-dev
RUN apt-get install -y libssl-dev
RUN apt-get install -y libleveldb-dev
RUN apt-get install -y libjsoncpp-dev
RUN apt-get install -y libsnappy-dev
RUN apt-get install -y libmicrohttpd-dev
RUN apt-get install -y libjsonrpccpp-dev
RUN apt-get install -y libminiupnpc-dev

#--------------------------------------------------------------------
# GET SOURCES
#--------------------------------------------------------------------
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG BRANCH=master

RUN git clone -b ${BRANCH} ${REPO}

#--------------------------------------------------------------------
# CONFIGURE
#--------------------------------------------------------------------
ARG CONFIG=Debug

WORKDIR Zilliqa
RUN cmake -DCMAKE_BUILD_TYPE=${CONFIG} .
RUN cmake --build .

ENTRYPOINT ["/bin/bash"]
