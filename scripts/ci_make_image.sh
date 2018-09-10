#!/bin/bash
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
