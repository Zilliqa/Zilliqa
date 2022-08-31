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
# This script is dedicated for CI use
#

set -x
set -e

export scilla_container_id=$(docker run -d --name zilliqa_scilla_$HOSTNAME zilliqa/scilla:v0.11.0 sleep infinity)
export ubuntu_container_id=$(docker run -d --name zilliqa_bionic_$HOSTNAME ubuntu:bionic sleep infinity)

docker cp $scilla_container_id:/scilla /scilla
docker cp /scilla $ubuntu_container_id:/scilla

docker cp /home/jenkins/ $ubuntu_container_id:/home/jenkins/

function remove_containers
{
    docker rm -f $1;
    docker rm -f $2;
}

( docker exec -i $ubuntu_container_id ls /home/jenkins/ && \
    docker exec -i $ubuntu_container_id /home/jenkins/agent/workspace/zilliqaci/scripts/setup_environment.sh && \
    remove_containers $scilla_container_id $ubuntu_container_id \
) || ( remove_containers $scilla_container_id $ubuntu_container_id )