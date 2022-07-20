#!/usr/bin/env python
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

import sys
import argparse
import requests
import os

FILE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def test_chainid(url: str) -> bool:
    try:
        response = requests.post(url, json={"id": "1", "jsonrpc": "2.0", "method": "eth_chainId"})

        if response.status_code != 200:
            raise Exception(f"Bad status code {response.status_code} - {response.text}")

        if "0x" not in response.json()["result"]:
            raise Exception(f"Bad json or response {response.status_code} - {response.text}")

    except Exception as e:
        print(f"Failed test test_chainid with error: '{e}'")
        return False

    return True


def parse_commandline():
    parser = argparse.ArgumentParser()
    parser.add_argument('--api', type=str, required=True, help='API to test against')
    return parser.parse_args()


def main():
    args = parse_commandline()

    print(f"args are {args.api}")

    if args.api[-1] != '/':
        args.api[-1].append('/')

    ret = test_chainid(args.api)
    ret |= test_chainid(args.api)

    if not ret:
        print(f"Test failed")
        sys.exit(1)


if __name__ == '__main__':
    main()
