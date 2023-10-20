#!/usr/bin/env python3
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

import argparse
import os
import shutil

from natlib.data import Data
from natlib.lib import (is_seedprv, \
                        generate_nodes, str2fanout, str2uints, str2lookup)

LOOKUP_TYPES = ('lookup', 'seedpub', 'seedprv')


def main():
    parser = argparse.ArgumentParser(description='initialize zilliqa')

    parser.add_argument('--pod-name', help='pod name (TESTNET-TYPE-INDEX)')
    parser.add_argument('--type', choices=['normal', 'lookup', 'new', 'seedprv'],
                        help='node type (normal, lookup, new, seedprv)')
    parser.add_argument('--index', type=int, help='node index')
    parser.add_argument('--metadata-url', help='the base URL for metadata')
    parser.add_argument('-n', type=int, help='number of all nodes', required=True)
    parser.add_argument('-d', type=int, help='number of ds nodes', required=True)
    parser.add_argument('-l', type=int, help='number of lookup nodes', required=True)
    parser.add_argument('--ds-guard', type=int, default=0, help='number of ds guards')
    parser.add_argument('--shard-guard', type=int, default=0, help='number of shard guards')
    parser.add_argument('--skip-non-guard-ds', action='store_true', default=False,
                        help='do not create pods for non-guard DS nodes')
    parser.add_argument('--out-dir', help='The output directory')
    parser.add_argument('--build-dir', help='The build directory')
    parser.add_argument('--block-number-size', default=64, type=int, choices=[64, 256],
                        help='block number size (64-bit, 256-bit)')
    # TODO: --seed-multiplier is redudant when --multiplier-fanout is used
    parser.add_argument('--seed-multiplier', action='store_true', default=True, help='Support seed multiplier')
    parser.add_argument('--multiplier-fanout', type=str2fanout, help='the multiplier fanouts')
    parser.add_argument('--max-recovery', default='2', help='Max tries for recovering zilliqa node')
    parser.add_argument('--resume', action='store_true', help='Resume from persistent storage')
    parser.add_argument('--recover-from-testnet', action='store_true', help='Recover all nodes from persistent storage')
    parser.add_argument('--transaction-sender', default='0', type=str2uints, metavar='N',
                        help='List of lookup indices that send testing transactions')
    parser.add_argument('--origin-server', help='set external origin server instead using in-cluster one')
    parser.add_argument('--lookup-dns-domain', help='the DNS name for lookup, multiplier, and seedpub nodes')
    parser.add_argument('--log-path', help='Set customized log path')
    parser.add_argument('--bucket', help='Set bucket name')
    parser.add_argument('--hosted-zone-id', help='hosted zone ID')
    parser.add_argument('--txnsbackup', action='store_true', help='Enable storing txns backup to S3')
    parser.add_argument('--websocket', type=str2lookup, metavar='TYPE1,TYPE2',
                        help='enable websocket for lookup server of TYPE, can be any of {}'.format(LOOKUP_TYPES))

    group0 = parser.add_argument_group('Host Network Mode')
    group0.add_argument('--port', type=int, help='port for zilliqa application')

    group1 = parser.add_argument_group('Local Testing Mode', 'for testing and debugging init.py')
    group1.add_argument('--testing', action='store_true', help='enable local testing mode')

    group2 = parser.add_argument_group('Private keys cleanup Mode',
                                       'for cleaning up non-self private keys before running the working container')
    group2.add_argument('--cleanup-private-keys', action='store_true', help='clean up non-self private keys')

    group3 = parser.add_argument_group('Native mode settings', 'for defining how native will behave')
    group3.add_argument('--isolated-server-accounts', action='store_true',
                        help='use the isolated server accounts file from zilliqa directory')

    args = parser.parse_args()

    # Inititalise the data object
    data = Data()

    # The origin server stores the network information
    if args.origin_server is not None:
        # if --origin-server is set, use it first
        data.origin_server = args.origin_server
    elif args.metadata_url is not None:
        # metadata dns record used to be created with nginx ingress installation, not now
        # If --metadata-url is set, use the internal one of origin
        data.origin_server = '{}/origin/{}'.format(args.metadata_url, args.testnet)
        # origin_server = args.metadata_url
    else:
        # This will not be used in the native implementation
        data.origin_server = ""
        print("We currently cannot start without something in origin server")
        return 0

    if args.isolated_server_accounts is not None:
        # Copy into configmap
        try:
            shutil.copyfile("../isolated-server-accounts.json",
                            os.path.join("/etc/zilliqa", "isolated-server-accounts.json"))
        except:
            print("Failed to copy isolated-server-accounts.json")
            print("you need to create a /etc/zilliqa directory with rw access ")
            return 0


    if not data.get_ips_list_from_pseudo_origin(args):
        print("Generating data from the command line arguments failed")
        return 0
    else:
        data.print_details()

    args.restart = False

    try:
        os.mkdir(args.out_dir)
    except FileExistsError:
        pass

    cwd = os.getcwd()

    generate_nodes(args, data, 'lookup', 0, args.l)
    os.chdir(cwd)
    generate_nodes(args, data, 'dsguard', 0, args.ds_guard)
    os.chdir(cwd)
    generate_nodes(args, data, 'normal', 0, args.d - args.ds_guard)
    os.chdir(cwd)
    generate_nodes(args, data, 'multiplier', 0, len(data.get_multiplier()))
    os.chdir(cwd)
    generate_nodes(args, data, 'seedpub', 0, sum(args.multiplier_fanout))

    return 0


# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    main()
