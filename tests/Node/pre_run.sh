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



rm -rf local_run/node*

sudo sysctl net.core.somaxconn=102400; 
sudo sysctl net.core.netdev_max_backlog=65536; 
sudo sysctl net.ipv4.tcp_tw_reuse=1; 
sudo sysctl -w net.ipv4.tcp_rmem='65536 873800 1534217728';
sudo sysctl -w net.ipv4.tcp_wmem='65536 873800 1534217728';
sudo sysctl -w net.ipv4.tcp_mem='65536 873800 1534217728';
ulimit -n 65535;
ulimit -Sc unlimited; 
ulimit -Hc unlimited;
ulimit -s unlimited; 

python tests/Zilliqa/test_zilliqa_local.py stop
python tests/Zilliqa/test_zilliqa_local.py setup 10
python tests/Zilliqa/test_zilliqa_local.py prestart 5

# clean up persistence storage
rm -rf lookup_local_run/node*

python tests/Zilliqa/test_zilliqa_lookup.py setup 1

