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
# Usage:
# 
#   run this script with 'watch':
#       watch -n1 tests/Node/watch_node_simple.sh'

for id in {1..20}
do
    port=$((5000 + $id))
    [ $id -lt 10 ] && id=0$id
    node_cmd_info=$(pgrep -f "zilliqa.*127\.0\.0\.1.*\-\-port ${port}.*" -a | cut -f1,5,6 -d" ")  
    node_log=$(tail -n1 local_run/node_00$id/zilliqa-00001-log.txt)
    if [[ -z $node_cmd_info ]]
    then
        node_cmd_info="dead"
        node_log=$(tail -n1 local_run/node_00$id/error_log_zilliqa)
    fi
    echo node $id: $node_cmd_info $node_log
done
