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
FROM zilliqa/scilla:v0.13.0 AS scilla

# run a copy -L to unfold the symlinkes, and strip all exes
RUN mkdir -p /scilla/0/bin2/ && cp -L /scilla/0/bin/* /scilla/0/bin2/ && strip /scilla/0/bin2/*

# start from a new ubuntu environment as builder for zilliqa, make sure the deps is consistent with those in the runner image
FROM zilliqa/zilliqa:v0.0.0-deps AS builder

# Prepare a (vcpkg) python environment separate from the system-wide one
RUN apt-get remove -y python3 cmake && apt-get autoremove -y

# Manually input tag or commit, can be overwritten by docker build-args
ARG COMMIT_OR_TAG=master
ARG REPO=https://github.com/Zilliqa/Zilliqa.git
ARG SOURCE_DIR=/zilliqa
ARG BUILD_DIR=/build
ARG INSTALL_DIR=/usr/local
ARG BUILD_TYPE=RelWithDebInfo
ARG EXTRA_CMAKE_ARGS=
ARG VCPKG_ROOT=/vcpkg

# If COMMIT_OR_TAG is a branch name or a tag, clone a shallow copy which is
# faster; if this fails, just clone the full repo and checkout the commit.
RUN git clone -b ${COMMIT_OR_TAG} --depth 1 ${REPO} ${SOURCE_DIR} \
  || git clone ${REPO} ${SOURCE_DIR}
RUN git -C ${SOURCE_DIR} checkout ${COMMIT_OR_TAG}
RUN cmake -G Ninja -H${SOURCE_DIR} -B${BUILD_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-dynamic ${EXTRA_CMAKE_ARGS} 
RUN cmake --build ${BUILD_DIR} --config ${BUILD_TYPE}
RUN cmake --build ${BUILD_DIR} --target install
RUN cd $SOURCE_DIR/evm-ds && PATH=${BUILD_DIR}/vcpkg_installed/x64-linux-dynamic/tools/protobuf:$PATH cargo build --release --package evm-ds
RUN ldd ${INSTALL_DIR}/bin/zilliqa | grep vcpkg_installed | gawk '{print $3}' | xargs -I{} cp {} ${INSTALL_DIR}/lib \
  && cp ${BUILD_DIR}/vcpkg_installed/x64-linux-dynamic/lib/libffi.so ${INSTALL_DIR}/lib 
RUN echo "built files:" && ls -lh ${BUILD_DIR} && echo "installed libs:" && ls -lh ${INSTALL_DIR}/lib

ARG VCPKG_PYTHON_PATH=${BUILD_DIR}/vcpkg_installed/x64-linux-dynamic/tools/python3
ARG VCPKG_PYTHON3=${VCPKG_PYTHON_PATH}/python3.10
ARG PYTHON3_ENV_PATH=${INSTALL_DIR}
RUN ${VCPKG_PYTHON3} -m ensurepip \
  && ${VCPKG_PYTHON3} -m pip install --upgrade pip \
  && mkdir -p ${PYTHON3_ENV_PATH}/bin \
  && mkdir -p ${PYTHON3_ENV_PATH}/lib \
  && cp ${VCPKG_PYTHON_PATH}/* ${PYTHON3_ENV_PATH}/bin/ \
  && ldd ${VCPKG_PYTHON3} | grep vcpkg_installed | gawk '{print $3}' | xargs -I{} cp {} ${INSTALL_DIR}/lib \
  && cp -R ${BUILD_DIR}/vcpkg_installed/x64-linux-dynamic/lib/python3.10 ${PYTHON3_ENV_PATH}/lib/

# strip all exes
RUN strip /usr/local/bin/grepperf \
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
   /usr/local/lib/libSchnorr.so \
   /usr/local/lib/libCryptoUtils.so \
   /usr/local/lib/libNAT.so \
   /usr/local/lib/libCommon.so \
   /usr/local/lib/libTrie.so

# make dirs for scilla and zilliqa
RUN mkdir -p \
    /scilla/0/bin /scilla/0/src/stdlib \
    /zilliqa/scripts

# pour in scilla binaries
COPY --from=scilla  /scilla/0/bin2            /scilla/0/bin
# pour in scilla conntract stdlibs
COPY --from=scilla  /scilla/0/src/stdlib     /scilla/0/src/stdlib

RUN cp ${SOURCE_DIR}/evm-ds/target/release/evm-ds ${INSTALL_DIR}/bin/ \
  && cp ${SOURCE_DIR}/evm-ds/log4rs.yml ${INSTALL_DIR}/etc/

ADD https://github.com/krallin/tini/releases/latest/download/tini /tini

COPY requirements3.txt ./
RUN update-alternatives --install /usr/bin/python python ${PYTHON3_ENV_PATH}/bin/python3.10 10 \ 
  && update-alternatives --install /usr/bin/python3 python3 ${PYTHON3_ENV_PATH}/bin/python3.10 10 # set python3 as default instead python3

ENV LD_LIBRARY_PATH=${INSTALL_DIR}/lib

RUN python3 -m pip install --upgrade pip setuptools wheel \
  && python3 -m pip install --no-cache-dir -r requirements3.txt

RUN echo '#!/bin/bash\npython3 -m pip "$@"' > /usr/bin/pip3 \
  && chmod a+x /usr/bin/pip3 \
  && ln -s /usr/bin/pip3 /usr/bin/pip

RUN rm -rf ${VCPKG_ROOT}
RUN rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["/bin/bash"]

