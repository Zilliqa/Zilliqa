#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your
# participation in testing Zilliqa. You may view, compile and run the code for
# that purpose and pursuant to the protocols and algorithms that are programmed
# into, and intended by, the code. You may not do anything else with the code
# without express permission from Zilliqa Research Pte. Ltd., including
# modifying or publishing the code (or any part of it), and developing or
# forming another public or private blockchain network. This source code is
# provided 'as is' and no warranties are given as to title or non-infringement,
# merchantability or fitness for purpose and, to the extent permitted by law,
# all liability for your use of the code is disclaimed. Some programs in this
# code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
# are governed by GPLv3.0 are those programs that are located in the folders
# src/depends and tests/depends and which include a reference to GPLv3 in their
# program files.
#
# The script is dedicated for CI use
#
# Usage:
#
#    ./scripts/ci_make_image.sh        # build with 2 jobs
#    ./scripts/ci_make_image.sh N      # build with N jobs
#

set -e

docker --version
aws --version

[ -n "$1" ] && jobs=$1 || jobs=2
commit=$(git rev-parse --short=7 ${TRAVIS_COMMIT})
account_id=$(aws sts get-caller-identity --output text --query 'Account')
region_id=us-east-1
registry_url=${account_id}.dkr.ecr.${region_id}.amazonaws.com/zilliqa:${commit}

eval $(aws ecr get-login --no-include-email --region ${region_id})
docker build --build-arg COMMIT=${commit} --build-arg JOBS=${jobs} -t zilliqa:${commit} docker
docker build -t ${registry_url} -<<EOF
FROM zilliqa:${commit}
RUN apt-get update && apt-get install -y --no-install-recommends \
    dnsutils \
    gdb \
    less \
    logrotate \
    net-tools \
    rsyslog \
    vim \
    && rm -rf /var/lib/apt/lists/* \
    && pip install setuptools \
    && pip install kubernetes
EOF

docker push ${registry_url}
