#
#
#  Copyright (C) 2019 Zilliqa
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.

import sys
import argparse
import requests

FILE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TEST_RETURN_STATUS = 0

def test_chainid(url: string):
    try:
        response = requests.get(url + "/chain_id")
    except Exception as e:
        print("Failed test test_chainid with error {e}")
        TEST_RETURN_STATUS = 1


print(f"File path is {FILE_PATH}")

def parse_commandline():
    parser = argparse.ArgumentParser()
    parser.add_argument('--api', type=str, help='API to test against')
    return parser.parse_args()


def main():
    args = parse_commandline()

    print(f"args are {args.api}")

    test_chainid(api)

    sys.exit(TEST_RETURN_STATUS)


if __name__ == '__main__':
    main()
