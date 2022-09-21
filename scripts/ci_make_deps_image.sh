#!/bin/bash
# Copyright (C) 2022 Zilliqa
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
#    ./scripts/ci_make_deps_image.sh
#
# Environment Varibles:
#
#    TRVAIS_COMMIT
#    - compulsory
#    - the commit to build
#    - TODO: change the variable name to be platform independent (e.g., CI_COMMIT)
#
#
# IMPORTANT: note that the built image will *not* depend on the commit and will *always*
#            be tagged based on the version in ../VERSION
set -e

docker --version
aws --version

major=$(tail -n +2 VERSION | head -n1)
minor=$(tail -n +4 VERSION | head -n1)
fix=$(tail -n +6 VERSION | head -n1)

commit_or_tag=$(git rev-parse --short=7 ${TRAVIS_COMMIT})
account_id=$(aws sts get-caller-identity --output text --query 'Account')
region_id=us-west-2

# See above comment about the image tag.
source_image=zilliqa:v${major}.${minor}.${fix}-deps
target_image=${account_id}.dkr.ecr.${region_id}.amazonaws.com/${source_image}

eval $(aws ecr get-login --no-include-email --region ${region_id})
set +e
make -C docker deps COMMIT_OR_TAG="${commit_or_tag}"  || exit 10
set -e
docker tag ${source_image} ${target_image}
docker push ${target_image}
