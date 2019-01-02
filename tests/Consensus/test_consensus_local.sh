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

python tests/Zilliqa/test_zilliqa_local.py stop
python tests/Zilliqa/test_zilliqa_local.py clean
python tests/Zilliqa/test_zilliqa_local.py setup 9
python tests/Zilliqa/test_zilliqa_local.py start
python tests/Zilliqa/test_zilliqa_local.py connect
python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 2 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 3 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 4 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 5 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 6 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 7 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 8 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 9 03000000
python tests/Zilliqa/test_zilliqa_local.py sendcmd 1 0301FF
