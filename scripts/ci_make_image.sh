#!/bin/bash
#
# The script is dedicated for CI use
#

set -e

docker --version
pip install --user awscli
export PATH=$PATH:$HOME/.local/bin

branch=${TRAVIS_BRANCH}
commit=$(git rev-parse --short ${TRAVIS_COMMIT})
account_id=$(aws sts get-caller-identity --output text --query 'Account')
region_id=us-east-1
registry_url=${account_id}.dkr.ecr.${region_id}.amazonaws.com/zilliqa:${commit}

eval $(aws ecr get-login --no-include-email --region ${region_id})
docker build --build-arg BRANCH=${branch} --build-arg COMMIT=${commit} \
    -t zilliqa:${commit} docker
docker build -t ${registry_url} -<<EOF
FROM zilliqa:${commit}
RUN apt-get update && apt-get install -y --no-install-recommends \
    dnsutils \
    net-tools \
    vim \
    && rm -rf /var/lib/apt/lists/*
EOF

docker push ${registry_url}

