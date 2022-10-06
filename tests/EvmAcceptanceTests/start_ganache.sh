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

set -e

clear
ganache -p 7545 -v --logging.debug --chain.networkId 3 \
    --wallet.accounts "0xc95690aed4461afd835b17492ff889af72267a8bdf7d781e305576cd8f7eb182,0xDE0B6B3A7640000" \
    --wallet.accounts "0x05751249685e856287c2b2b9346e70a70e1d750bc69a35cef740f409ad0264ad,0xDE0B6B3A7640000" \
    --wallet.accounts "0xe7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411,0xDE0B6B3A7640000" \
    --wallet.accounts "0x410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14,0xDE0B6B3A7640000"
