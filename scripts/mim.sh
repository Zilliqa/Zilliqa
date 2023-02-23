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

export KIND_IP_ADDR=$(docker container inspect zqdev-control-plane --format '{{ .NetworkSettings.Networks.kind.IPAddress }}')
echo "starting man in the middle"
mitmweb --mode reverse:http://$KIND_IP_ADDR --modify-headers '/~q/Host/l2api.local.z7a.xyz' --set web_port=8082 --no-web-open-browser&
mitmweb --mode reverse:http://localhost -p 8081 --set keep_host_header=true --set web_port=8085 --no-web-open-browser&