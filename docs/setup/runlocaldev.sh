#! /usr/bin/bash
# Copyright (C) 2023 Zilliqa
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

source ~/setenv.sh
cd ~/src/zilliqa/zilliqa
./scripts/localdev.py setup
IP=`minikube ip`
cat <<EOF | sudo tee -a /etc/hosts
${IP} localdev-api.localdomain
${IP} localdev-explorer.localdomain
${IP} localdev-l2api.localdomain
${IP} localdev-newapi.localdomain
${IP} localdev-origin.localdomain
${IP} localdev-origin-internal.localdomain
EOF
./scripts/localdev.py up
