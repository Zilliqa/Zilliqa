#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import base64
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.cElementTree as xtree
from datetime import datetime, timedelta
from os import path
from urllib.parse import urlparse

import boto3
import netaddr
import requests

############################# constants ########################################
ELB_WAITING_TIME_IN_MINUTES = 5
TESTNET_READINESS_TIME_IN_MINUTES = 300
lookup_rpc_port = None


############################# utility functions ################################

def readline_from_file(filename):
    with open(filename, 'r') as f:
        return [line.strip() for line in f.readlines()]



def get_my_keypair(args):
    if is_dsguard(args):
        return args.keypairs[args.index]

    if is_normal(args):
        offset = args.d if args.skip_non_guard_ds else args.ds_guard
        return args.keypairs[offset + args.index]

    if is_lookup(args):
        return args.lookup_keypairs[args.index]

    if is_seedpub(args):
        return args.seedpub_keypairs[args.index]

    if is_new(args) and len(args.new_keypairs) > args.index:
        return args.new_keypairs[args.index]

    if args.testing:
        return '02C197AB58D4B2F11691341BCAB8A4F7E7FDFFE756EF24D0AF066103DAEFD121E1 4B7DDC27B56407EBF8D1B124A2E79EFBE920E0BAB80AB361D6CCEA7C4B135BB8'

    return subprocess.check_output('genkeypair').decode().strip()


def get_my_ip_and_port(args):
    if is_dsguard(args):
        return args.normal_ips[args.index]

    if is_normal(args):
        offset = args.d if args.skip_non_guard_ds else args.ds_guard
        return args.normal_ips[offset + args.index]

    if is_lookup(args):
        return args.lookup_ips[args.index]

    if is_seedpub(args):
        return args.seedpub_ips[args.index]

    return (None, None)


def redact_private_key(filename, ignore_index=None):
    """
    original keys.txt:
        abc def
        hij klm
        nop qrs
    start_private_key('keys.txt', ignore_index=1)
    processed keys.txt:
        abc *
        hij klm
        nop *
    """
    out = []
    with open(filename) as f:
        for i, pair in enumerate(f):
            if i == ignore_index:
                out.append(pair.strip())
            else:
                out.append(pair.split()[0] + ' *')
    with open(filename, 'w') as f:
        f.write('\n'.join(out))


KEY_FILES = ['keys.txt', 'lookup_keys.txt', 'seedpub_keys.txt', 'new_keys.txt', 'multiplier_keys.txt']


def clean_non_self_private_keys(args):
    """
    redact all non-self private keys
    """
    index = args.index
    self_file = ''

    if is_normal(args):
        self_file = 'keys.txt'
        # recalculate 'index' in the same way in `get_my_keypair()`
        offset = args.d if args.skip_non_guard_ds else args.ds_guard
        index = offset + args.index

    if is_dsguard(args):
        self_file = 'keys.txt'

    if is_lookup(args):
        self_file = 'lookup_keys.txt'

    if is_seedpub(args):
        self_file = 'seedpub_keys.txt'

    if is_new(args) and len(args.new_keypairs) > args.index:
        self_file = 'new_keys.txt'

    if is_multiplier(args):
        # speical case: multiplier doesn't need its own private key
        self_file = ''

    print("start cleaning non-self private keys")
    print("self type: %s, key file: %s, key index: %s" % (args.type, self_file, index))
    for key_file in KEY_FILES:
        key_file_path = path.join(args.conf_dir, 'secret', key_file)
        if not path.isfile(key_file_path):
            continue
        if key_file == self_file:
            print("redacting private keys (without index %s) of key file: %s" % (index, key_file))
            redact_private_key(key_file_path, index)
        else:
            print("redacting all private keys of key file: " + key_file)
            redact_private_key(key_file_path)
    print("done")


def get_my_aws_ipv4(args):  # works on AWS
    if args.testing:
        return '169.254.169.254'

    # https://www.safaribooksonline.com/library/view/regular-expressions-cookbook/9780596802837/ch07s16.html
    ipv4_re = r"^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"

    # https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-instance-addressing.html
    response = urllib.request.urlopen('http://169.254.169.254/latest/meta-data/public-ipv4')
    ipv4 = response.read().decode()

    match = re.match(ipv4_re, ipv4)
    if match and match.group(0) == ipv4:
        return ipv4

    print('Cannnot get valid public address: got {}'.format(ipv4))
    return ''


# A pod may be one of the following type
# - lookup
# - dsguard (only exist when guards are used)
# - normal
#   * ds or non-ds
#   * shard_guard
# - multiplier
# - new
# - seedprv

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


# pod map for nodes of type normal and/or dsguard
#
# notations:
# * N -> normal pod
# * D -> dsguard pod
# * d -> args.d
# * n -> args.n
# * dg -> args.ds_guard
# * sg -> args.shard_guard

# --------------------------------------------------------------------------------------------------------------------------
# | line # in keys.txt | guard mode disabled  | guard mode enabled  | guard mode, non-guard DS skipped | is_* == True
# --------------------------------------------------------------------------------------------------------------------------
# | 0                  | N 0                  | D 0                 | D 0                              | is_ds, is_ds_guard
# | 1                  | N 1                  | D 1                 | D 1                              | is_ds, is_ds_guard
# | 2                  | N 2                  | D 2                 | D 2                              | is_ds, is_ds_guard
# | ...                | ...                  | ...                 | ...                              | is_ds, is_ds_guard
# | (dg - 1)           | N (dg - 1)           | D (dg - 1)          | D (dg - 1)                       | is_ds, is_ds_guard
# --------------------------------------------------------------------------------------------------------------------------
# | (dg)               | N (dg)               | N 0                 | skipped                          | is_ds,
# | (dg + 1)           | N (dg + 1)           | N 1                 | skipped                          | is_ds,
# | ...                | ...                  | ...                 | ...                              | is_ds,
# | (d - 1)            | N (d - 1)            | N (d - dg - 1)      | skipped                          | is_ds,
# --------------------------------------------------------------------------------------------------------------------------
# | (d)                | N (d)                | N (d - dg)          | N 0                              | is_non_ds, is_shard_guard
# | d + 1              | N (d + 1)            | N (d - dg + 1)      | N 1                              | is_non_ds, is_shard_guard
# | ...                | ...                  | ...                 | ...                              | is_non_ds, is_shard_guard
# | d + sg - 1         | N (d + sg - 1)       | N (d - dg + sg - 1) | N (sg - 1)                       | is_non_ds, is_shard_guard
# --------------------------------------------------------------------------------------------------------------------------
# | d + sg             | N (d + sg)           | N (d - dg + sg)     | N (sg)                           | is_non_ds,
# | d + sg + 1         | N (d + sg + 1)       | N (d - dg + sg + 1) | N (sg + 1)                       | is_non_ds,
# | ...                | ...                  | ...                 | ...                              | is_non_ds,
# | (n - 1)            | N (n - 1)            | N (n - dg - 1)      | N (n - d - 1)                    | is_non_ds,

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


def create_constants_xml(args):
    root = xtree.parse(path.join(args.conf_dir, 'configmap', 'constants.xml')).getroot()

    lookup_nodes = root.find('lookups')
    lookup_ips = args.lookup_ips[0: args.l]
    lookup_public_keys = [k.split(' ')[0] for k in args.lookup_keypairs[0: args.l]]

    multiplier_ips = filter_empty_ip(args.multiplier_ips)[0: len(args.multiplier_fanout)]
    multiplier_public_keys = [k.split(' ')[0] for k in args.multiplier_keypairs[0: len(args.multiplier_fanout)]]

    seedpub_ips = args.seedpub_ips[0: sum(args.multiplier_fanout)]
    seedpub_public_keys = [k.split(' ')[0] for k in args.seedpub_keypairs[0: sum(args.multiplier_fanout)]]

    upper_seed_ip = []
    upper_seed_pubkey = []
    upper_seed_index = []

    l2l_data_provider_ip = []
    l2l_data_provider_pubkey = []
    l2l_data_provider_index = []

    # Upper seed for lookup = all lookups
    if is_lookup(args):
        upper_seed_ip = lookup_ips
        upper_seed_pubkey = lookup_public_keys
        upper_seed_index = list(range(len(lookup_ips)))
    # Upper seed for seedprv / seed-configuration.tar.gz = all seedpubs
    elif is_seedprv(args):
        upper_seed_ip = seedpub_ips[0:sum(args.multiplier_fanout)]
        upper_seed_pubkey = seedpub_public_keys[0:sum(args.multiplier_fanout)]
        upper_seed_index = list(range(0, sum(args.multiplier_fanout)))
        l2l_data_provider_ip = seedpub_ips[0:sum(args.multiplier_fanout)]
        l2l_data_provider_pubkey = seedpub_public_keys[0:sum(args.multiplier_fanout)]
        l2l_data_provider_index = list(range(0, sum(args.multiplier_fanout)))
    # Upper seed for seedpub = last X lookups
    elif is_seedpub(args):
        if args.l >= 4:
            upper_seed_ip = lookup_ips[2:args.l]
            upper_seed_pubkey = lookup_public_keys[2:args.l]
            upper_seed_index = list(range(2, args.l))
        else:
            upper_seed_ip = [lookup_ips[args.l - 1]]
            upper_seed_pubkey = [lookup_public_keys[args.l - 1]]
            upper_seed_index = [args.l - 1]
    # Upper seed for dsguard / normal / configuration.tar.gz = all seedpubs (if available) or all lookups
    else:
        if args.seed_multiplier:
            upper_seed_ip = seedpub_ips[0:sum(args.multiplier_fanout)]
            upper_seed_pubkey = seedpub_public_keys[0:sum(args.multiplier_fanout)]
            upper_seed_index = list(range(0, sum(args.multiplier_fanout)))
        else:
            upper_seed_ip = lookup_ips
            upper_seed_pubkey = lookup_public_keys
            upper_seed_index = list(range(len(lookup_ips)))

    upper_seed_nodes = root.find('upper_seed')

    for (index, ip, pubkey) in zip(range(len(lookup_ips)), lookup_ips, lookup_public_keys):
        lookup_peer = xtree.SubElement(lookup_nodes, "peer")
        xtree.SubElement(lookup_peer, "ip").text = ip[0]
        xtree.SubElement(lookup_peer, "pubkey").text = pubkey
        xtree.SubElement(lookup_peer, "port").text = str(ip[1])

        if args.lookup_dns_domain is not None:
            dns = '{testnet}-lookup-{index}.{domain}'.format(testnet=args.testnet, index=index,
                                                             domain=args.lookup_dns_domain)
            xtree.SubElement(lookup_peer, "hostname").text = dns
        else:
            xtree.SubElement(lookup_peer, "hostname").text = ""

    # upper seed takes peers from lookups, the DNS names are the same
    for (index, ip, pubkey) in zip(upper_seed_index, upper_seed_ip, upper_seed_pubkey):
        upper_seed_peer = xtree.SubElement(upper_seed_nodes, "peer")
        xtree.SubElement(upper_seed_peer, "ip").text = ip[0]
        xtree.SubElement(upper_seed_peer, "pubkey").text = pubkey
        xtree.SubElement(upper_seed_peer, "port").text = str(ip[1])

        if args.lookup_dns_domain is not None:
            if args.seed_multiplier and not (is_seedpub(args) or is_lookup(args)):
                dns = '{testnet}-seedpub-{index}.{domain}'.format(testnet=args.testnet, index=index,
                                                                  domain=args.lookup_dns_domain)
            else:
                dns = '{testnet}-lookup-{index}.{domain}'.format(testnet=args.testnet, index=index,
                                                                 domain=args.lookup_dns_domain)
            xtree.SubElement(upper_seed_peer, "hostname").text = dns
        else:
            xtree.SubElement(upper_seed_peer, "hostname").text = ""

    l2l_data_providers = root.find('l2l_data_providers')
    # l2l data providers takes peers from lookups, the DNS names are the same
    for (index, ip, pubkey) in zip(l2l_data_provider_index, l2l_data_provider_ip, l2l_data_provider_pubkey):
        l2l_data_provider_peer = xtree.SubElement(l2l_data_providers, "peer")
        xtree.SubElement(l2l_data_provider_peer, "ip").text = ip[0]
        xtree.SubElement(l2l_data_provider_peer, "pubkey").text = pubkey
        xtree.SubElement(l2l_data_provider_peer, "port").text = str(ip[1])

        dns = ""
        if args.lookup_dns_domain is not None:
            if args.seed_multiplier and is_seedprv(args):
                dns = '{testnet}-seedpub-{index}.{domain}'.format(testnet=args.testnet, index=index,
                                                                  domain=args.lookup_dns_domain)
        xtree.SubElement(l2l_data_provider_peer, "hostname").text = dns

    multiplier_nodes = root.find('multipliers')
    for (index, ip, key) in zip(list(range(len(multiplier_ips))), multiplier_ips, multiplier_public_keys):
        multiplier_peer = xtree.SubElement(multiplier_nodes, "peer")
        xtree.SubElement(multiplier_peer, "ip").text = ip[0]
        xtree.SubElement(multiplier_peer, "port").text = str(ip[1])
        xtree.SubElement(multiplier_peer, "pubkey").text = key

        if args.lookup_dns_domain is not None:
            dns = '{testnet}-multiplier-{index}.{domain}'.format(testnet=args.testnet, index=index,
                                                                 domain=args.lookup_dns_domain)
            xtree.SubElement(multiplier_peer, "hostname").text = dns
        else:
            xtree.SubElement(multiplier_peer, "hostname").text = ""

    if is_lookup(args) or is_seedpub(args) or is_seedprv(args):
        general = root.find('general')
        general.find('LOOKUP_NODE_MODE').text = "true"

        jsonrpc = root.find('jsonrpc')
        global lookup_rpc_port
        if lookup_rpc_port == None:
            lookup_rpc_port = int(jsonrpc.find('LOOKUP_RPC_PORT').text)

        jsonrpc.find('LOOKUP_RPC_PORT').text = str(lookup_rpc_port)
        lookup_rpc_port = lookup_rpc_port + 1

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


def create_ds_whitelist_xml(args):
    # public key + ip + port
    normal_public_keys = [k.split(' ')[0] for k in args.keypairs[0: args.n]]
    normal_ips = args.normal_ips[0: args.n]

    nodes = xtree.Element("nodes")
    for (ip, pubkey) in zip(normal_ips, normal_public_keys):
        peer = xtree.SubElement(nodes, "peer")
        xtree.SubElement(peer, "pubk").text = pubkey
        xtree.SubElement(peer, "ip").text = ip[0]
        xtree.SubElement(peer, "port").text = str(ip[1])

    tree = xtree.ElementTree(nodes)
    tree.write("ds_whitelist.xml")


def create_shard_whitelist_xml(args):
    # public key
    normal_public_keys = [k.split(' ')[0] for k in args.keypairs[0: args.n]]

    nodes = xtree.Element("address")
    for pubkey in normal_public_keys:
        xtree.SubElement(nodes, "pubk").text = pubkey

    tree = xtree.ElementTree(nodes)
    tree.write("shard_whitelist.xml")


def create_config_xml(args):
    if is_new(args):
        # create a config.xml with 0 ds node information when I am a new node
        ds_public_keys = []
        ds_ips = []
    else:
        ds_public_keys = [k.split(' ')[0] for k in args.keypairs[0: args.d]]
        ds_ips = args.normal_ips[0: args.d]

    nodes = xtree.Element("nodes")
    for (ip, pubkey) in zip(ds_ips, ds_public_keys):
        peer = xtree.SubElement(nodes, "peer")
        xtree.SubElement(peer, "pubk").text = pubkey
        xtree.SubElement(peer, "ip").text = ip[0]
        xtree.SubElement(peer, "port").text = str(ip[1])

    tree = xtree.ElementTree(nodes)
    tree.write("config.xml")


def create_multiplier_start_sh(listen_port, lookupips_url):
    start_sh = [
        '#!/bin/bash',
        'echo "Starting multiplier"',
        'echo "Listening on port {}"'.format(listen_port),
        'echo "Lookup IPs URL: {}"'.format(lookupips_url),
        'echo "Starting multiplier"',
        './asio_multiplier --listen "{}" --url "{}"'.format(listen_port, lookupips_url),
    ]

    with open('start.sh', 'w') as f:
        for line in start_sh:
            f.write(line + '\n')


def create_dsnodes_xml(args):
    ds_public_keys = [k.split(' ')[0] for k in args.keypairs[0: args.d]]
    dsnode = xtree.Element("dsnodes")
    for pubkey in ds_public_keys:
        xtree.SubElement(dsnode, "pubk").text = pubkey
    tree = xtree.ElementTree(dsnode)
    tree.write("dsnodes.xml")


SED = "sed -i " + ('""' if sys.platform == 'darwin' else '')


def gen_testnet_sed_string(args, fileName):
    return f'{SED} "s,^TESTNET_NAME=.*$,TESTNET_NAME= \'{args.testnet}\'," {fileName}'


def gen_bucket_sed_string(args, fileName):
    return f'{SED} "s,^BUCKET_NAME=.*$,BUCKET_NAME= \'{args.bucket}\'," {fileName}'


def create_start_sh(args):
    block0 = '0' * int(args.block_number_size / 4 - 1) + '1'  # 000....001
    ds_diff = "05"  # genesis ds diff
    diff = "03"  # genesis diff
    rand1 = "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a"
    rand2 = "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf"
    my_public_key, my_private_key = get_my_keypair(args).strip().split(' ')
    my_ip, my_port = get_my_ip_and_port(args)
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
            '--logpath {}'.format(args.log_path) if args.log_path is not None else ''
        ])

    if is_normal(args) or is_dsguard(args):
        primary_ds_ip = args.normal_ips[0]
        cmd_setprimaryds = ' '.join([
            'sendcmd',
            '--port {}'.format(my_port),
            '--cmd cmd',
            '--cmdarg 0100' + ip_to_hex(primary_ds_ip[0]) + '{0:08X}'.format(primary_ds_ip[1])
        ])
    else:
        cmd_setprimaryds = ''

    if is_non_ds(args):
        ds_public_keys = [k.split(' ')[0] for k in args.keypairs[0: args.d]]
        ds_ips = args.normal_ips[0: args.d]

        cmd_startpow = ' '.join([
            'sendcmd',
            '--port {}'.format(my_port),
            '--cmd cmd',
            '--cmdarg 0200' + block0 + ds_diff + diff + rand1 + rand2 + ''.join(
                [
                    ds[0] + ip_to_hex(ds[1][0]) + '{0:08X}'.format(ds[1][1])
                    for ds in zip(ds_public_keys, ds_ips)
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
        'export AWS_ENDPOINT_URL=http://127.0.0.1:4566',
        'export PATH=/run/zilliqa:$PATH',
        defer_cmd(cmd_setprimaryds, 20) if is_ds(args) and not args.recover_from_testnet else '',
        defer_cmd(cmd_startpow, 40) if is_non_ds(args) and not args.recover_from_testnet else '',
        # Actually launch the binary now
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

    return my_ns


def wait_for_aws_elb_ready(name):
    api_instance = client.CoreV1Api()

    while True:
        try:
            ret = api_instance.read_namespaced_service(namespace='default', name=name)
            delta = datetime.utcnow() - ret.metadata.creation_timestamp.replace(tzinfo=None)
            delta_seconds = delta.total_seconds()
            if delta < timedelta(minutes=ELB_WAITING_TIME_IN_MINUTES):
                print(
                    "Waiting {:.1f} seconds until the load-balancer of service {} is ready".format(delta_seconds, name))
                time.sleep(delta_seconds)
            else:
                print("Load-balancer is up {:.1f} seconds before".format(delta_seconds))
            break
        except ApiException as e:
            print('Exception when getting service {}: {}'.format(name, e))
        except Exception as e:
            print('Exception when getting the creation time of service {}: {}'.format(name, e))
        finally:
            sys.stdout.flush()
            time.sleep(10)


def is_restarted(deploy_name):
    api_instance = client.AppsV1Api()

    while True:
        try:
            ret = api_instance.read_namespaced_deployment(namespace='default', name=deploy_name, )
            delta = datetime.utcnow() - ret.metadata.creation_timestamp.replace(tzinfo=None)
            delta_seconds = delta.total_seconds()
            print("This pod starts {:.1f} seconds after {}".format(delta_seconds, deploy_name))
            if delta < timedelta(minutes=TESTNET_READINESS_TIME_IN_MINUTES):
                print("This run is NOT considered as restart")
                return False
            else:
                print("This run is considered as restart")
                return True
        except ApiException as e:
            print('Exception when getting the creation time of deployment {}: {}'.format(deploy_name, e))
        finally:
            sys.stdout.flush()
            time.sleep(10)


def wait_for_statefulset_ready(name):
    api_instance = client.AppsV1Api()

    while True:
        try:
            ret = api_instance.read_namespaced_stateful_set(namespace='default', name=name, exact=True)
        except ApiException as e:
            print('Exception when getting statefulset {}: {}'.format(name, e))
        else:
            print('Checking statefulset {} status: {}/{}'.format(name, ret.status.ready_replicas, ret.spec.replicas))
            if ret.status.ready_replicas == ret.spec.replicas or ret.spec.replicas == 0:
                break
        finally:
            sys.stdout.flush()
            time.sleep(10)


def get_ingress_url(name):
    api_instance = client.ExtensionsV1beta1Api()
    https_annote_key = "external-dns.alpha.kubernetes.io/hostname"
    while True:
        try:
            ret = api_instance.read_namespaced_ingress(name=name, namespace='default')
            annotations = ret.metadata.annotations
        except ApiException as e:
            print('Exception when getting ingress {}: {}'.format(name, e))
        except Exception as e:
            print('Exception when getting ingress {}: {}'.format(name, e))
        else:
            if annotations is not None and https_annote_key in annotations:
                return 'https://{}'.format(annotations[https_annote_key])
            print("Could not find annotation '{}' in ingress '{}'".format(https_annote_key, name))
        finally:
            sys.stdout.flush()
            time.sleep(10)


def get_svc_ip(name):
    """
    get k8s service (same namespace) ip from envVars
    :type name: str
    """
    env_name = name.upper().replace('-', '_') + '_SERVICE_HOST'
    return os.getenv(env_name)


def get_pods_locations(testnet, typename):
    """Return a list of tuple in the format (POD_NAME, NODE_NAME, POD_IP)
    """
    api_instance = client.CoreV1Api()

    while True:
        try:
            ret = api_instance.list_namespaced_pod(namespace='default',
                                                   label_selector='testnet={},type={}'.format(testnet, typename))
        except ApiException as e:
            print('Exception when fetching {}-{} pods information: {}'.format(testnet, typename, e))
        else:
            return [(i.metadata.name, i.spec.node_name, i.status.pod_ip) for i in ret.items]
        finally:
            sys.stdout.flush()
            time.sleep(10)


def get_basic_auth_secret(secret_name):
    api_instance = client.CoreV1Api()

    while True:
        try:
            ret = api_instance.read_namespaced_secret(name=secret_name, namespace='default')
            username = base64.b64decode(ret.data['username']).decode()
            password = base64.b64decode(ret.data['password']).decode()
        except ApiException as e:
            print('Exception when fetching secret {}: {}'.format(secret_name, e))
        else:
            return (username, password)
        finally:
            sys.stdout.flush()
            time.sleep(10)


def get_node_ips():
    api_instance = client.CoreV1Api()

    def get_external_ip(addresses):
        for a in addresses:
            if a.type == 'ExternalIP':
                return a.address

        # assert False, "Should have 'ExternalIP' {}".format(addresses)

    while True:
        try:
            ret = api_instance.list_node()
        except ApiException as e:
            print('Exception when getting node information: {}'.format(e))
        else:
            rt = {}
            for i in ret.items:
                ip = get_external_ip(i.status.addresses)
                if ip:
                    rt[i.metadata.name] = ip
            return rt
        finally:
            sys.stdout.flush()
            time.sleep(10)


def get_basic_auth_link(url, username, password):
    o = urlparse(url)
    auth_url = '{}://{}:{}@{}'.format(o.scheme, username, password, o.netloc + o.path)
    return auth_url


def get_ip_list_from_origin(url, resource_name, start_port):
    ips = readline_from_file(url + '/' + resource_name)
    ips = ["127.0.0.1"] * len(ips)
    return list(zip(ips, range(start_port, start_port + len(ips))))


def generate_ip_mapping_file(ips, keypairs, port, n):
    normal_public_keys = [k.split(' ')[0] for k in keypairs[0: n]]

    with open("ipMapping.txt", 'w') as f:
        f.write('<mapping>\n')
        for i in range(n):
            f.write('<peer><ip>' + ips[i] + '</ip><port>' + str(port + i) + '</port><pubkey>' + normal_public_keys[
                i] + '</pubkey></peer>\n')
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


# this nonsense needs an origin server which is now simply a directory we will automagically create
# however as we need port numbers as well as IP addresses we will generate them first and then let the stupid code
# continue blindly along.
def create_origin_contents(ip, port, multiplier, gaurds, normal, seedpub):
    if port < 0 or port > 65535:
        raise argparse.ArgumentTypeError("port must be between 0 and 65535")

    return True


def main():
    parser = argparse.ArgumentParser(description='initialize zilliqa')

    parser.add_argument('--pod-name', help='pod name (TESTNET-TYPE-INDEX)')
    parser.add_argument('--testnet', help='testnet name')
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
    parser.add_argument('--conf-dir', default='/etc/zilliqa', help='The path of the mounted configmap')
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

    args = parser.parse_args()

    args.keypairs = readline_from_file(path.join(args.conf_dir, 'secret', 'keys.txt'))
    if path.isfile(path.join(args.conf_dir, 'secret', 'new_keys.txt')):
        args.new_keypairs = readline_from_file(path.join(args.conf_dir, 'secret', 'new_keys.txt'))
    else:
        args.new_keypairs = []
    args.lookup_keypairs = readline_from_file(path.join(args.conf_dir, 'secret', 'lookup_keys.txt'))
    args.multiplier_keypairs = readline_from_file(path.join(args.conf_dir, 'secret', 'multiplier_keys.txt'))
    args.seedpub_keypairs = readline_from_file(path.join(args.conf_dir, 'secret', 'seedpub_keys.txt'))

    if args.cleanup_private_keys:
        clean_non_self_private_keys(args)
        return 0

    # The origin server stores the network information
    if args.origin_server is not None:
        # if --origin-server is set, use it first
        origin_server = args.origin_server
    elif args.metadata_url is not None:
        # metadata dns record used to be created with nginx ingress installation, not now
        # If --metadata-url is set, use the internal one of origin
        origin_server = '{}/origin/{}'.format(args.metadata_url, args.testnet)
        # origin_server = args.metadata_url
    else:
        origin_server = 'http://' + get_svc_ip('{}-origin'.format(args.testnet))

    first_available_port = args.port
    lookup_ips_from_origin = get_ip_list_from_origin(origin_server, 'lookup_ips.txt', first_available_port)
    first_available_port = first_available_port + len(lookup_ips_from_origin)
    normal_ips_from_origin = get_ip_list_from_origin(origin_server, 'normal_ips.txt', first_available_port)
    first_available_port = first_available_port + len(normal_ips_from_origin)
    if args.recover_from_testnet:
        generate_ip_mapping_file(normal_ips_from_origin, args.keypairs, first_available_port, args.n)

    multiplier_ips_from_origin = get_ip_list_from_origin(origin_server, 'multiplier_ips.txt',
                                                         first_available_port) if args.seed_multiplier else []
    first_available_port = first_available_port + len(multiplier_ips_from_origin)
    seedpub_ips_from_origin = get_ip_list_from_origin(origin_server, 'seedpub_ips.txt',
                                                      first_available_port) if args.seed_multiplier else []

    first_available_port += 1

    args.restart = False

    if is_seedprv(args):
        args.verifier_keypair = subprocess.check_output('genkeypair').decode().strip()
        # generate verifier file
        with open("verifier.txt", 'w') as f:
            f.write(args.verifier_keypair)

    def generate_files(pod_name):
        if is_lookup(args) or is_dsguard(args) or is_normal(args) or is_seedprv(args) or is_multiplier(args):
            args.normal_ips = normal_ips_from_origin
            args.lookup_ips = lookup_ips_from_origin
        elif is_new(args) or is_seedpub(args):
            args.normal_ips = []
            args.lookup_ips = lookup_ips_from_origin

        #  if args.seed_multiplier and (is_lookup(args) or is_dsguard(args) or is_normal(args) or is_new(args)):
        args.multiplier_ips = multiplier_ips_from_origin
        args.seedpub_ips = seedpub_ips_from_origin
        #  else:
        #  args.multiplier_ips = []
        #  if is_seedprv(args):
        #  args.seedpub_ips = seedpub_ips_from_origin
        #  else:
        #  args.seedpub_ips = []

        create_constants_xml(args)
        if is_normal(args) or is_dsguard(args):
            create_ds_whitelist_xml(args)
            create_shard_whitelist_xml(args)
        create_config_xml(args)
        create_dsnodes_xml(args)
        return create_start_sh(args)

    def generate_nodes(node_type, first_index: int, count: int):
        node_nss = []
        scripts_dir = os.path.dirname(os.path.abspath(__file__))
        zilliqa_dir = os.path.abspath(os.path.join(scripts_dir, '..', '..', 'zilliqa'))
        scilla_dir = os.path.abspath(os.path.join(scripts_dir, '..', '..', 'scilla'))

        for index in range(first_index, first_index + count):
            try:
                pod_name = f'{args.testnet}-{node_type}-{index}'
                pod_path = os.path.join(args.out_dir, pod_name)

                try:
                    os.mkdir(pod_path)
                except FileExistsError:
                    pass

                os.chdir(pod_path)
                os.system("rm zilliqa.log")

                args.type = node_type
                args.index = index
                args.port += 1

                if (node_type != 'multiplier'):
                    node_nss.append(generate_files(pod_name))
                else:
                    create_new_multiplier_file(origin_server, args, seedpub_ips_from_origin)
                    multi_basic_auth_url = '{}/multiplier-downstream.txt'.format("http://0.0.0.0:8000")
                    create_multiplier_start_sh(multiplier_ips_from_origin[0][1], multi_basic_auth_url)

                sed_extra_arg = '-i ""' if sys.platform == "darwin" else '-i'
                os.system(f'sed {sed_extra_arg} -e "s,/run/zilliqa,{pod_path}," -e "s,/zilliqa/scripts,{scripts_dir}," start.sh')
                os.system(f'sed {sed_extra_arg} -e "s,<SCILLA_ROOT>.*</SCILLA_ROOT>,<SCILLA_ROOT>{scilla_dir}</SCILLA_ROOT>," -e "s,<EVM_SERVER_BINARY>.*</EVM_SERVER_BINARY>,<EVM_SERVER_BINARY>{zilliqa_dir}/evm-ds/target/debug/evm-ds</EVM_SERVER_BINARY>," -e "s,<EVM_LOG_CONFIG>.*</EVM_LOG_CONFIG>,<EVM_LOG_CONFIG>{zilliqa_dir}/evm-ds/log4rs.yml</EVM_LOG_CONFIG>," -e "s,\.sock\>,-{node_type}.{index}.sock," constants.xml')

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

        return node_nss

    try:
        os.mkdir(args.out_dir)
    except FileExistsError:
        pass

    cwd = os.getcwd()

    nss = []
    # This is the starting port for the zilliqad process it will get incremented each time.
    starting_port: int = args.port

    nss = nss + generate_nodes('lookup', 0, args.l)
    nss = nss + generate_nodes('dsguard', 0, args.ds_guard)
    nss = nss + generate_nodes('normal', 0, args.d - args.ds_guard)
    nss = nss + generate_nodes('normal', args.d - args.ds_guard, args.n - args.d)
    nss = nss + generate_nodes('seedpub', 0, sum(args.multiplier_fanout))
    nss = nss + generate_nodes('multiplier', 0, 1)


    return 0


def create_new_multiplier_file(url, args, address_list):
    try:
        os.remove("multiplier-downstream.txt")
    except FileNotFoundError:
        print("No multiplier-downstream.txt file found - creating new one")

    with open("multiplier-downstream.txt", 'w') as f:
        for addr in address_list:
            f.write(addr[0] + ":" + str(addr[1]) + "\n")
    return True

def save_ip_list(filename, ip_list):
    with open(filename, 'w') as f:
        f.writelines([line + '\n' for line in ip_list])


if __name__ == "__main__":
    main()
