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
import socket
import struct
import subprocess
import sys
import xml.etree.cElementTree as xtree
from os import path

import netaddr

lookup_rpc_port = None


def get_my_keypair(args, data) -> str:
    if is_dsguard(args):
        t = data.get_guard()
        t2 = t[args.index]
        t3 = t2[data.PUBLIC]
        return t3

    if is_normal(args):
        t = data.get_normal()
        t2 = t[args.index]
        t3 = t2[data.PUBLIC]
        return t3

    if is_lookup(args):
        t = data.get_lookup()
        t2 = t[args.index]
        t3 = t2[data.PUBLIC]
        return t3

    if is_seedpub(args):
        t = data.get_seedpub()
        t2 = t[args.index]
        t3 = t2[data.PUBLIC]
        return t3

    if is_new(args) and len(args.new_keypairs) > args.index:
        return subprocess.check_output('genkeypair').decode().strip()

    if args.testing:
        return '02C197AB58D4B2F11691341BCAB8A4F7E7FDFFE756EF24D0AF066103DAEFD121E1 4B7DDC27B56407EBF8D1B124A2E79EFBE920E0BAB80AB361D6CCEA7C4B135BB8'

    return subprocess.check_output('genkeypair').decode().strip()



def get_my_ip_and_port(args, zil_data):

    if is_dsguard(args):
        return zil_data.guard_ips_from_origin[args.index][:2]

    if is_normal(args):
        return zil_data.normal_ips_from_origin[args.index][:2]

    if is_lookup(args):
        return zil_data.lookup_ips_from_origin[args.index][:2]

    if is_seedpub(args):
        return zil_data.seedpub_ips_from_origin[args.index][:2]

    if is_multiplier(args):
        return zil_data.multiplier_ips_from_origin[args.index][:2]

    return (None, None)



def is_lookup(args):
    return args.type == 'lookup'


def is_seedpub(args):
    return args.type == 'seedpub'


def is_multiplier(args):
    return args.type == 'multiplier'


def is_normal(args):
    return args.type == 'normal'


def is_new(args):
    return args.type == 'new'


def is_seedprv(args):
    return args.type == 'seedprv'


def is_ds(args):
    if args.type == 'dsguard':
        return True

    if args.type == 'normal':
        # If non-guard DS nodes are skipped, all normal nodes fall into non-DS
        if args.skip_non_guard_ds:
            return False
        # Else, check the index to see if it's DS node
        return args.index < args.d - args.ds_guard

    # other types are definitely not DS
    return False


def is_non_ds(args):
    if args.type == 'normal':
        # if non-guard DS nodes are skipped, all normal nodes fall into non-DS
        if args.skip_non_guard_ds:
            return True
        # Else, check the index
        return args.index >= args.d - args.ds_guard

    return False


def is_dsguard(args):
    return args.type == 'dsguard'


def is_shard_guard(args):
    if args.type == 'normal':
        offset = args.d if args.skip_non_guard_ds else args.ds_guard
        return offset + args.index >= args.d and offset + args.index < args.d + args.shard_guard

    return False


################################ functions ####################################
def filter_empty_ip(ip_list):
    result = []
    for ip in ip_list:
        if ip != "":
            result.append(ip)
    return result


def create_constants_xml(args, data):
    base_dir = "../../.."
    root = xtree.parse(path.join(base_dir,  'constants.xml')).getroot()

    lookup_nodes = root.find('lookups')

    upper_seed = []
    l2l_data = []

    if is_lookup(args):
        upper_seed = data.get_lookup()
    elif is_seedprv(args):
        upper_seed = data.get_seedpub()
        l2l_data_provider = data.get_seedpub()
    elif is_seedpub(args):
        if args.l >= 4:
            upper_seed = data.get_lookup()[2:args.l]
        else:
            upper_seed = [data.get_lookup()[args.l - 1]]
    else:
        if args.seed_multiplier:
            upper_seed = data.get_seedpub()[0:sum(args.multiplier_fanout)]
        else:
            upper_seed = data.get_lookup()

    upper_seed_nodes = root.find('upper_seed')

    offset = 0
    for item in data.get_lookup():
        lookup_peer = xtree.SubElement(lookup_nodes, "peer")
        xtree.SubElement(lookup_peer, "ip").text = item[data.IP]
        xtree.SubElement(lookup_peer, "pubkey").text = item[data.PUBLIC].split(' ')[0]
        xtree.SubElement(lookup_peer, "port").text = str(item[data.PORT])

        if args.lookup_dns_domain is not None:
            dns = '{testnet}-lookup-{index}.{domain}'.format(testnet="native", index=offset,
                                                             domain=args.lookup_dns_domain)
            xtree.SubElement(lookup_peer, "hostname").text = dns
        else:
            xtree.SubElement(lookup_peer, "hostname").text = ""
        offset += 1

    # upper seed takes peers from lookups, the DNS names are the same
    offset = 0
    for item in upper_seed:
        upper_seed_peer = xtree.SubElement(upper_seed_nodes, "peer")
        xtree.SubElement(upper_seed_peer, "ip").text = item[data.IP]
        xtree.SubElement(upper_seed_peer, "pubkey").text = item[data.PUBLIC].split(' ')[0]
        xtree.SubElement(upper_seed_peer, "port").text = str(item[data.PORT])

        if args.lookup_dns_domain is not None:
            if args.seed_multiplier and not (is_seedpub(args) or is_lookup(args)):
                dns = '{testnet}-seedpub-{index}.{domain}'.format(testnet="native", index=offset,
                                                                  domain=args.lookup_dns_domain)
            else:
                dns = '{testnet}-lookup-{index}.{domain}'.format(testnet="native", index=offset,
                                                                 domain=args.lookup_dns_domain)
            xtree.SubElement(upper_seed_peer, "hostname").text = dns
        else:
            xtree.SubElement(upper_seed_peer, "hostname").text = ""
        offset += 1

    l2l_data_providers = root.find('l2l_data_providers')
    # l2l data providers takes peers from lookups, the DNS names are the same
    offset = 0
    for item in l2l_data:
        l2l_data_provider_peer = xtree.SubElement(l2l_data_providers, "peer")
        xtree.SubElement(l2l_data_provider_peer, "ip").text = item[data.IP]
        xtree.SubElement(l2l_data_provider_peer, "pubkey").text = item[data.PUBLIC].split(' ')[0]
        xtree.SubElement(l2l_data_provider_peer, "port").text = str(item[data.PORT])

        dns = ""
        if args.lookup_dns_domain is not None:
            if args.seed_multiplier and is_seedprv(args):
                dns = '{testnet}-seedpub-{index}.{domain}'.format(testnet="native", index=offset,
                                                                  domain=args.lookup_dns_domain)
        xtree.SubElement(l2l_data_provider_peer, "hostname").text = dns
        offset += 1

    multiplier_nodes = root.find('multipliers')
    offset = 0
    for item in data.get_multiplier():

        multiplier_peer = xtree.SubElement(multiplier_nodes, "peer")
        xtree.SubElement(multiplier_peer, "ip").text = item[data.IP]
        xtree.SubElement(multiplier_peer, "port").text = str(item[data.PORT])
        xtree.SubElement(multiplier_peer, "pubkey").text = item[data.PUBLIC].split(' ')[0]

        if args.lookup_dns_domain is not None:
            dns = '{testnet}-multiplier-{index}.{domain}'.format(testnet="native", index=offset,
                                                                 domain=args.lookup_dns_domain)
            xtree.SubElement(multiplier_peer, "hostname").text = dns
        else:
            xtree.SubElement(multiplier_peer, "hostname").text = ""
        offset += 1

    # new code to move the status and lookup port away from the default

    my_ip, my_port = get_my_ip_and_port(args, data)

    if my_port is not None:
        if is_lookup(args) or is_seedpub(args) or is_seedprv(args):
            general = root.find('general')
            general.find('LOOKUP_NODE_MODE').text = "true"

            jsonrpc = root.find('jsonrpc')
            global lookup_rpc_port
            if lookup_rpc_port == None:
                lookup_rpc_port = int(jsonrpc.find('LOOKUP_RPC_PORT').text)

            jsonrpc.find('LOOKUP_RPC_PORT').text = str(int(my_port)+1)
            lookup_rpc_port = lookup_rpc_port + 1

    ''' New Configuration for native'''

    guardmode = root.find('guard_mode')
    if guardmode:
        guardmode.find('GUARD_MODE').text = "true"

    transactions = root.find('transactions')
    if transactions:
        transactions.find('BUCKET_NAME').text = "native"
        transactions.find('TXN_PERSISTENCE_NAME').text = "zilliqa"

    ''' configure SCILLA_MULTI_VERSION correctly '''

    scilla = root.find('smart_contract')
    if scilla:
        scilla.find('ENABLE_SCILLA_MULTI_VERSION').text = "false"
        scilla.find('LOG_SC').text = "true"



    jsonrpc = root.find('jsonrpc')
    if my_port is not None:
        status_rpc_port = int(my_port) + 2
        jsonrpc.find('STATUS_RPC_PORT').text = str(status_rpc_port)
    else:
        print("my_port is None")

    transactions = root.find('transactions')
    if is_lookup(args) or is_seedpub(args) or is_seedprv(args):
        transactions.find('ENABLE_REPOPULATE').text = "true"
    else:
        transactions.find('ENABLE_REPOPULATE').text = "false"

    if (is_dsguard(args) or is_shard_guard(args)) and args.txnsbackup:
        transactions.find('ENABLE_TXNS_BACKUP').text = "true"
    else:
        transactions.find('ENABLE_TXNS_BACKUP').text = "false"

    if is_seedpub(args):
        seed = root.find('seed')
        seed.find('ARCHIVAL_LOOKUP').text = "true"
        seed.find('ENABLE_SEED_TO_SEED_COMMUNICATION').text = "true"
    else:
        seed = root.find('seed')
        seed.find('ENABLE_SEED_TO_SEED_COMMUNICATION').text = "false"

    jsonrpc = root.find("jsonrpc")
    if jsonrpc and jsonrpc.find("ENABLE_STAKING_RPC") is not None:
        if is_seedpub(args):
            jsonrpc.find("ENABLE_STAKING_RPC").text = "true"
        else:
            jsonrpc.find("ENABLE_STAKING_RPC").text = "false"

    if jsonrpc and jsonrpc.find("ENABLE_WEBSOCKET") is not None:
        if args.websocket and args.type in args.websocket:
            jsonrpc.find("ENABLE_WEBSOCKET").text = "true"
        else:
            jsonrpc.find("ENABLE_WEBSOCKET").text = "false"

    if is_seedprv(args):
        seed = root.find('seed')
        seed.find('ARCHIVAL_LOOKUP').text = "true"

        jsonrpc = root.find('jsonrpc')
        jsonrpc.find('ENABLE_GETTXNBODIESFORTXBLOCK').text = "true"

    # To enable txn generation for seedprv, uncomment line below and comment other following line
    # if is_seedprv(args) or (is_lookup(args) and args.transaction_sender == args.index):
    if is_lookup(args) and args.index in args.transaction_sender:
        dispatcher = root.find('dispatcher')
        dispatcher.find('USE_REMOTE_TXN_CREATOR').text = "true"

    # strip off private keys as nodes shouldn't see them
    accounts = root.find('accounts')
    if accounts is not None:
        for account in accounts.findall('account'):
            private_key = account.find('private_key')
            if private_key is not None:
                account.remove(private_key)
    ds_accounts = root.find('ds_accounts')
    if ds_accounts is not None:
        for ds_account in ds_accounts.findall('account'):
            private_key = ds_account.find('private_key')
            if private_key is not None:
                ds_account.remove(private_key)

    tree = xtree.ElementTree(root)
    tree.write('constants.xml')


def create_ds_whitelist_xml(args, data):

    nodes = xtree.Element("nodes")
    for item in data.get_normal():
        peer = xtree.SubElement(nodes, "peer")
        xtree.SubElement(peer, "pubk").text = item[data.PUBLIC].split(' ')[0]
        xtree.SubElement(peer, "ip").text = item[data.IP]
        xtree.SubElement(peer, "port").text = str(item[data.PORT])

    tree = xtree.ElementTree(nodes)
    tree.write("ds_whitelist.xml")


def create_shard_whitelist_xml(data):
    # public key
    normal = data.get_normal()

    nodes = xtree.Element("address")
    for item in normal:
        xtree.SubElement(nodes, "pubk").text = item[data.PUBLIC].split(' ')[0]

    tree = xtree.ElementTree(nodes)
    tree.write("shard_whitelist.xml")


def create_config_xml( args, data):
    if is_new(args):
        # create a config.xml with 0 ds node information when I am a new node
        ds = []
        ds = []
    else:
        ds = data.get_normal()

    nodes = xtree.Element("nodes")
    for item in ds:
        peer = xtree.SubElement(nodes, "peer")
        xtree.SubElement(peer, "pubk").text = item[data.PUBLIC].split(' ')[0]
        xtree.SubElement(peer, "ip").text = item[data.IP]
        xtree.SubElement(peer, "port").text = str(item[data.PORT])

    tree = xtree.ElementTree(nodes)
    tree.write("config.xml")


def create_dsnodes_xml(args, data):
    ds = data.get_normal()
    dsnode = xtree.Element("dsnodes")
    for item in ds:
        xtree.SubElement(dsnode, "pubk").text = item[data.PUBLIC].split(' ')[0]
    tree = xtree.ElementTree(dsnode)
    tree.write("dsnodes.xml")


SED = "sed -i " + ('""' if sys.platform == 'darwin' else '')


def gen_testnet_sed_string(args, fileName):
    return f'{SED} "s,^TESTNET_NAME=.*$,TESTNET_NAME= \'{"native"}\'," {fileName}'


def gen_bucket_sed_string(args, fileName):
    return f'{SED} "s,^BUCKET_NAME=.*$,BUCKET_NAME= \'{args.bucket}\'," {fileName}'


def create_start_sh(args, zil_data):
    block0 = '0' * int(args.block_number_size / 4 - 1) + '1'  # 000....001
    ds_diff = "05"  # genesis ds diff
    diff = "03"  # genesis diff
    rand1 = "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a"
    rand2 = "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf"
    test = get_my_keypair(args, zil_data)
    my_public_key, my_private_key = test.strip().split(' ')
    my_ip, my_port = get_my_ip_and_port(args, zil_data)
    my_ns = 'zil-ns-{}'.format(int(my_ip[my_ip.rfind('.') + 1:]))

    # hex string from IPv4 or IPv6 string
    def ip_to_hex(ip):
        ip = int(netaddr.IPAddress(ip))
        ip = '{0:08X}'.format(ip)
        ip = hex(struct.unpack('>I', struct.pack('<I', int(ip, 16)))[0])
        return '{0:032X}'.format(int(ip, 16))

    def defer_cmd(cmd, seconds):
        return 'sleep {} && {} &'.format(seconds, cmd)

    # FIXME(#313): Before Zilliqa daemon is integrated, there's no way to recover a DS node
    def cmd_zilliqa_daemon(args, recovery=False, resume=False):
        """generate the command for invoking zilliqa binary

        Arguments:
        - args: the parsed cli options
        - recovery: keep recovering the node after crashes if set to True
        - resume: resume the node from persistent storage if set to True

        Returns:
        - the string with the right binary and its options
        """

        binary_name = 'zilliqad'

        # SyncType, 0 for no, 1 for new, 2 for normal, 3 for ds, 4 for lookup, 6 for seedprv
        opt_recovery = '0'
        if args.restart and is_dsguard(args):
            opt_sync_type = '7'
            opt_recovery = '0'
        elif args.recover_from_testnet is True and is_new(args) is False and is_seedprv(args) is False:
            opt_sync_type = '5'
            opt_recovery = '1'
        else:
            if resume:
                opt_sync_type = '0'
            elif recovery:
                if is_new(args):
                    opt_sync_type = '1'
                elif is_normal(args) or is_dsguard(args):
                    # FIXME(#313): normal node has to use non-DS sync type ('2') regardless of its DS involvement
                    opt_sync_type = '2'
                elif is_lookup(args) or is_seedpub(args):
                    opt_sync_type = '4'
                elif is_seedprv(args):
                    opt_sync_type = '6'
                else:
                    raise RuntimeError("Should not be here")
            elif is_new(args):
                opt_sync_type = '1'
                opt_recovery = '1'
            elif is_seedprv(args):
                opt_sync_type = '6'
                opt_recovery = '1'
            else:
                opt_sync_type = '0'

            if resume:
                opt_recovery = '1'
            elif recovery:
                opt_recovery = '1'

        return ' '.join([
            binary_name,
            '--privk {}'.format(my_private_key),
            '--pubk {}'.format(my_public_key),
            '--address {}'.format(my_ip),
            '--port {}'.format(my_port),
            '--synctype {}'.format(opt_sync_type),
            '--nodetype {}'.format(args.type),
            '--nodeindex {}'.format(args.index),
            '--recovery' if opt_recovery == '1' else '',
            '--logpath {}'.format(args.log_path) if args.log_path is not None else '',
            '--killnone'
        ])

    if is_normal(args) or is_dsguard(args):
        primary_ds_ip = zil_data.get_normal()[0][zil_data.IP]
        cmd_setprimaryds = ' '.join([
            './sendcmd',
            '--port {}'.format(my_port),
            '--cmd cmd',
            '--cmdarg 0100' + ip_to_hex(zil_data.get_normal()[0][zil_data.IP]) + '{0:08X}'.format(zil_data.get_normal()[0][zil_data.PORT])
        ])
    else:
        cmd_setprimaryds = ''


    if is_non_ds(args):
        cmd_startpow = ' '.join([
            './sendcmd',
            '--port {}'.format(my_port),
            '--cmd cmd',
            '--cmdarg 0200' + block0 + ds_diff + diff + rand1 + rand2 + ''.join(
                [
                    ds[zil_data.PUBLIC].split(' ')[0] + ip_to_hex(ds[zil_data.IP]) + '{0:08X}'.format(ds[zil_data.PORT]) for ds in zil_data.get_normal()
                ]
            )
        ])
    else:
        cmd_startpow = ''

    start_sh = [
        '#!/bin/bash',
        'cp /zilliqa/scripts/upload_incr_DB.py /run/zilliqa/upload_incr_DB.py' if is_lookup(args) or is_seedpub(
            args) or is_dsguard(args) else '',
        'cp /zilliqa/scripts/download_incr_DB.py /run/zilliqa/download_incr_DB.py',
        'chmod u+x /run/zilliqa/download_incr_DB.py',
        'cp /zilliqa/scripts/download_static_DB.py /run/zilliqa/download_static_DB.py',
        'chmod u+x /run/zilliqa/download_static_DB.py',
        'o1=$(grep INCRDB_DSNUMS_WITH_STATEDELTAS /run/zilliqa/constants.xml | sed -e \'s,^[^<]*<[^>]*>\\([^<]*\\)<[^>]*>.*$,\\1,\')',
        f'[ ! -z "$o1" ] && {SED} "s,^NUM_DSBLOCK=.*$,NUM_DSBLOCK= $o1," /run/zilliqa/upload_incr_DB.py' if is_lookup(
            args) or is_seedpub(args) or is_dsguard(args) else '',
        f'[ ! -z "$o1" ] && {SED} "s,^NUM_DSBLOCK=.*$,NUM_DSBLOCK= $o1," /run/zilliqa/download_incr_DB.py',
        'o1=$(grep NUM_FINAL_BLOCK_PER_POW /run/zilliqa/constants.xml | sed -e \'s,^[^<]*<[^>]*>\\([^<]*\\)<[^>]*>.*$,\\1,\')',
        f'[ ! -z "$o1" ] && {SED} "s,^NUM_FINAL_BLOCK_PER_POW=.*$,NUM_FINAL_BLOCK_PER_POW= $o1," /run/zilliqa/upload_incr_DB.py' if is_lookup(
            args) or is_seedpub(args) or is_dsguard(args) else '',
        f'[ ! -z "$o1" ] && {SED} "s,^NUM_FINAL_BLOCK_PER_POW=.*$,NUM_FINAL_BLOCK_PER_POW= $o1," /run/zilliqa/download_incr_DB.py',
        gen_testnet_sed_string(args, "/run/zilliqa/upload_incr_DB.py") if is_lookup(args) or is_seedpub(
            args) or is_dsguard(args) else '',
        gen_bucket_sed_string(args, "/run/zilliqa/upload_incr_DB.py") if is_lookup(args) or is_seedpub(
            args) or is_dsguard(args) else '',
        'chmod u+x /run/zilliqa/upload_incr_DB.py' if is_lookup(args) or is_seedpub(args) or is_dsguard(args) else '',
        'cp /zilliqa/scripts/auto_backup.py /run/zilliqa/auto_backup.py' if is_lookup(args) or is_seedpub(
            args) or is_dsguard(args) else '',
        gen_testnet_sed_string(args, "/run/zilliqa/auto_backup.py") if is_lookup(args) or is_seedpub(
            args) or is_dsguard(args) else '',
        gen_bucket_sed_string(args, "/run/zilliqa/auto_backup.py") if is_lookup(args) or is_seedpub(args) or is_dsguard(
            args) else '',
        'chmod u+x /run/zilliqa/auto_backup.py' if is_lookup(args) or is_seedpub(args) or is_dsguard(args) else '',
        # 'pip3 install ' + ('--user ' if sys.platform == 'darwin' else '') + 'requests clint',
        'storage_path=$(grep STORAGE_PATH /run/zilliqa/constants.xml | sed -e \'s,^[^<]*<[^>]*>\\([^<]*\\)<[^>]*>.*$,\\1,\')' if is_new(
            args) or is_seedprv(args) else '',
        gen_testnet_sed_string(args, "/run/zilliqa/download_incr_DB.py"),
        gen_bucket_sed_string(args, "/run/zilliqa/download_incr_DB.py"),
        gen_testnet_sed_string(args, "/run/zilliqa/download_static_DB.py"),
        gen_bucket_sed_string(args, "/run/zilliqa/download_static_DB.py"),
        'export AWS_ENDPOINT_URL=http://0.0.0.0:4566',
        'export PATH=/run/zilliqa:$PATH',
        defer_cmd(cmd_setprimaryds, 20) if is_ds(args) and not args.recover_from_testnet else '',
        defer_cmd(cmd_startpow, 40) if is_non_ds(args) and not args.recover_from_testnet else '',
        cmd_zilliqa_daemon(args, resume=args.resume),
        '[ "$1" != "--recovery" ] && exit 1',
        '# The followings are recovery sequences'
    ]

    cmd_recovery = [
        'touch recovery',
        'for i in {{1..{}}}'.format(args.max_recovery),
        'do',
        'echo $(($(<recovery)+1)) >recovery',  # increase the readiness counter
        'echo "Previous run failed, recovering $(<recovery) time(s) ..."',
        cmd_zilliqa_daemon(args, recovery=True),
        'done',
        'exit 1'
    ]

    start_sh.extend(cmd_recovery)

    with open('start.sh', 'w') as f:
        for line in start_sh:
            f.write(line + '\n')

    with open('setprimary.sh', 'w') as f:
        f.write(cmd_setprimaryds + '\n')

    with open('startpow.sh', 'w') as f:
        f.write(cmd_startpow + '\n')

    return my_ns



def generate_ip_mapping_file(data):

    with open("ipMapping.txt", 'w') as f:
        f.write('<mapping>\n')
        for item in data.get_normal():
            f.write('<peer><ip>' + item[data.IP] + '</ip><port>' + str(item[data.PORT]) + '</port><pubkey>' + item[data.PUBLIC].split(' ')[0] + '</pubkey></peer>\n')
        for item in data.get_guard():
            f.write('<peer><ip>' + item[data.IP] + '</ip><port>' + str(item[data.PORT]) + '</port><pubkey>' +
                    item[data.PUBLIC].split(' ')[0] + '</pubkey></peer>\n')
        f.write('</mapping>')


# Get default index via the hostname TESTNET-TYPE-INDEX
# This doesn't work when hostNetwork is used
def default_index():
    return int(socket.gethostname().rsplit('-', 1)[1])


def str2fanout(s):
    """Convert the string into an array of numbers representing number of seedpub nodes under each multiplier

    Example:
        '0': 1 multiplier with 0 seedpub nodes
        '1,2': 2 multipliers with 3 seedpub nodes
        '5,5,5,0,0': 5 multipliers with 15 seedpub nodes
    """
    try:
        fanout = [int(x) for x in s.split(',')]
    except Exception as _:
        raise argparse.ArgumentTypeError("'{}' is not a valid fanout parameter")
    return fanout


def str2uints(s):
    return str2fanout(s)


LOOKUP_TYPES = ('lookup', 'seedpub', 'seedprv')


def str2lookup(s):
    if not s:
        return []
    splits = s.split(',')
    for typ in splits:
        if typ not in LOOKUP_TYPES:
            raise argparse.ArgumentTypeError("Unknown lookup type {}. Should be one of {}".format(typ, LOOKUP_TYPES))
    return splits


def generate_files(args, data, pod_name):
    create_constants_xml(args, data)
    if is_normal(args) or is_dsguard(args):
        create_ds_whitelist_xml(args, data)
        create_shard_whitelist_xml(data)
    create_config_xml(args, data)
    create_dsnodes_xml(args, data)
    return create_start_sh(args, data)

def generate_nodes(args, zil_data, node_type, first_index, count) -> bool:
    cwd = os.getcwd()
    scripts_dir = os.path.abspath(os.path.join(cwd, '../scripts'))
    zilliqa_dir = os.path.abspath(os.path.join(scripts_dir, '../'))
    scilla_dir = os.path.abspath(os.path.join(scripts_dir, '../..',  'scilla'))
    for index in range(first_index, first_index + count):
        try:
            pod_name = f'{"native"}-{node_type}-{index}'
            pod_path = os.path.join(args.out_dir, pod_name)

            try:
                os.mkdir(pod_path)
            except FileExistsError:
                pass

            os.chdir(pod_path)

            args.type = node_type
            args.index = index

            if (node_type != 'multiplier'):
                generate_files(args, zil_data, pod_name)
            else:
                create_constants_xml(args, zil_data)
                multi_basic_auth_url = '{}/multiplier-downstream.txt'.format("http://0.0.0.0:8000")
                create_multiplier_start_sh(zil_data.multiplier_port, multi_basic_auth_url)
                create_new_multiplier_file(zil_data)

            # generate one just because we can

            if (node_type == 'normal'):
                generate_ip_mapping_file(zil_data)

            print(f'Generated files for pod_path: {pod_path}')
            print(f'Generated files for scripts_dir: {scripts_dir}')


            sed_extra_arg = '-i ""' if sys.platform == "darwin" else '-i'
            os.system(
                f'sed {sed_extra_arg} -e "s,/run/zilliqa,{pod_path}," -e "s,/zilliqa/scripts,{scripts_dir}," start.sh')

            if (node_type != 'multiplier'):
                try:
                    os.system(
                        f'sed {sed_extra_arg} -e "s,<SCILLA_ROOT>.*</SCILLA_ROOT>,<SCILLA_ROOT>{scilla_dir}</SCILLA_ROOT>," -e "s,<EVM_SERVER_BINARY>.*</EVM_SERVER_BINARY>,<EVM_SERVER_BINARY>{zilliqa_dir}/evm-ds/target/debug/evm-ds</EVM_SERVER_BINARY>," -e "s,<EVM_LOG_CONFIG>.*</EVM_LOG_CONFIG>,<EVM_LOG_CONFIG>{zilliqa_dir}/evm-ds/log4rs.yml</EVM_LOG_CONFIG>," -e "s,\.sock\>,-{node_type}.{index}.sock," constants.xml')
                except Exception as e:  # noqa
                    print(f'Failed to replace constants.xml: {e}')
                    return False

            for file_name in ['zilliqa', 'zilliqad', 'sendcmd', 'asio_multiplier']:
                try:
                    os.remove(file_name)
                except FileNotFoundError:
                    pass
                try:
                    os.link(os.path.join(args.build_dir, 'bin', file_name), file_name)
                except FileExistsError:
                    pass

        finally:
            os.chdir(os.getcwd())
    return True


def create_multiplier_start_sh(listen_port, lookupips_url):
    start_sh = [
        '#!/bin/bash',
        'echo "Starting multiplier"',
        'echo "Listening on port {}"'.format(listen_port),
        'echo "Lookup IPs URL: {}"'.format(lookupips_url),
        'echo "Starting multiplier"',
        './asio_multiplier -l "{}" -s "{}"'.format(listen_port, lookupips_url),
    ]

    with open('start.sh', 'w') as f:
        for line in start_sh:
            f.write(line + '\n')

def create_new_multiplier_file(zil_data) -> bool:
    try:
        os.remove("{}/multiplier-downstream.txt".format(zil_data.origin_server))
    except FileNotFoundError:
        pass

    with open("{}/multiplier-downstream.txt".format(zil_data.origin_server), 'w') as f:
        for node in zil_data.seedpub_ips_from_origin:
            f.write(node[0] + ":" + str(node[1]) + "\n")
    return True
