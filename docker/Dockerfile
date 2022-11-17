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
ARG EXTRA_CMAKE_ARGS=
ARG VCPKG_ROOT=/vcpkg

# If COMMIT_OR_TAG is a branch name or a tag, clone a shallow copy which is
# faster; if this fails, just clone the full repo and checkout the commit.
RUN git clone -b ${COMMIT_OR_TAG} --depth 1 ${REPO} ${SOURCE_DIR} \
  || git clone ${REPO} ${SOURCE_DIR}
RUN git -C ${SOURCE_DIR} checkout ${COMMIT_OR_TAG}

# Build
RUN cd ${SOURCE_DIR} && ./build.sh ninja

# Install
RUN cmake --build "${BUILD_DIR}" --target install

# Compile & Install EVM
RUN cd $SOURCE_DIR/evm-ds && PATH=${BUILD_DIR}/vcpkg_installed/x64-linux-dynamic/tools/protobuf:$PATH cargo build --release --package evm-ds

# Make sure all DLLs / shared libraries are installed
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

COPY requirements3.txt ./
RUN update-alternatives --install /usr/bin/python python ${PYTHON3_ENV_PATH}/bin/python3.10 10 \ 
  && update-alternatives --install /usr/bin/python3 python3 ${PYTHON3_ENV_PATH}/bin/python3.10 10 # set python3 as default instead python3

RUN python3 -m pip install --upgrade pip setuptools wheel \
  && python3 -m pip install --no-cache-dir -r requirements3.txt

RUN echo '#!/bin/bash\npython3 -m pip "$@"' > /usr/bin/pip3 \
  && chmod a+x /usr/bin/pip3 \
  && ln -s /usr/bin/pip3 /usr/bin/pip

ENTRYPOINT ["/bin/bash"]

