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
#
# The script is dedicated for CI use
#
# Usage:
#
#    ./scripts/ci_make_image.sh

set -e

docker --version
aws --version

commit=$(git rev-parse --short=7 ${TRAVIS_COMMIT})
account_id=$(aws sts get-caller-identity --output text --query 'Account')
region_id=us-west-2
source_image=zilliqa:${commit}
target_image=${account_id}.dkr.ecr.${region_id}.amazonaws.com/zilliqa:${commit}

eval $(aws ecr get-login --no-include-email --region ${region_id})
make -C docker k8s COMMIT=${commit}
docker tag ${source_image} ${target_image}
docker push ${target_image}
