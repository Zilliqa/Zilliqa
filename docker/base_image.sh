#!/bin/bash
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

set -e

apt-get update \
    && apt-get install -y software-properties-common \
    && apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    bison \
    build-essential \
    ca-certificates \
    ccache \
    clang-format \
    clang-tidy \
    cron \
    curl \
    dnsutils \
    gawk \
    gdb \
    git \
    htop \
    iproute2 \
    lcov \
    libcurl4-openssl-dev \
    libgmp-dev \
    libsecp256k1-dev \
    libssl-dev \
    libtool \
    libxml2-utils \
    logrotate \
    net-tools \
    ninja-build \
    nload \
    ocl-icd-opencl-dev \
    openssh-client \
    pkg-config \
    python3-dev \
    python3-pip \
    rsync \
    rsyslog \
    tar \
    trickle \
    unzip \
    vim \
    wget \
    zip \
    && apt-get remove -y cmake python2  && apt-get autoremove -y