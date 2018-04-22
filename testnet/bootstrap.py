#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your participation in
# testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
# the protocols and algorithms that are programmed into, and intended by, the code. You may
# not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
# including modifying or publishing the code (or any part of it), and developing or forming
# another public or private blockchain network. This source code is provided ‘as is’ and no
# warranties are given as to title or non-infringement, merchantability or fitness for purpose
# and, to the extent permitted by law, all liability for your use of the code is disclaimed.
# Some programs in this code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
# GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
# and which include a reference to GPLv3 in their program files.

from __future__ import print_function

import argparse
import datetime
import glob
import json
import multiprocessing
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import traceback
import yaml

import xml.etree.cElementTree as xmlTree
# current dir
testnet_dir = os.path.dirname(os.path.abspath(__file__))

# project root dir
root_dir = os.path.dirname(testnet_dir)

# default build dir
build_dir = os.path.join(root_dir, 'build')

def populate_local_filetree(args):
    template_dir = os.path.join(testnet_dir, args.base)

    try:
        shutil.copytree(template_dir, args.rundir)
    except OSError as e:
        print(e)
        return

    print('Running dir:', args.rundir)

    util_dir = os.path.join(build_dir, 'bin')

    genkeypair = os.path.join(util_dir, 'genkeypair')

    keypairs = []
    for _ in range(args.n):
        process = subprocess.Popen([genkeypair], stdout=subprocess.PIPE)
        (output, err) = process.communicate()
        exit_code = process.wait()
        keypairs.append(output)

    keypairs.sort()
    print(keypairs)

    nodes = xmlTree.Element("nodes")

    # Store sorted keys list in text file
    keys_file = open(os.path.join(args.rundir, 'keys.txt'), "w")

    base_port = 5000
    for x in range(args.n):
        keys_file.write(keypairs[x])
        keypair = keypairs[x].rstrip('\n').split(" ")
        if (x < args.d):
            peer = xmlTree.SubElement(nodes, "peer")
            xmlTree.SubElement(peer, "pubk").text = keypair[0]
            xmlTree.SubElement(peer, "ip").text = '127.0.0.1'
            xmlTree.SubElement(peer, "port").text = str(base_port + x)
    keys_file.close()

    # Create config.xml with pubkey and IP info of all DS nodes
    tree = xmlTree.ElementTree(nodes)
    tree.write(os.path.join(args.rundir, "config.xml"))

def default_rundir(prefix=''):
    return prefix + '_' + datetime.datetime.now().strftime('%Y-%m-%d_%H%M%S')

def main():
    parser = argparse.ArgumentParser(description='bootstrap a local/cloud testnet')
    parser.add_argument('-b', '--base', metavar='STRING', default='local',
        choices=['local', 'cloud'], help='testnet base location')
    parser.add_argument('-r', '--rundir', metavar='PATH',
        help='the root dir for storing config and persistent data')
    parser.add_argument('-n', type=int, default=20, help='testnet total nodes')
    parser.add_argument('-d', type=int, default=10, help='number of ds nodes')
    parser.add_argument('--bin', default=build_dir, help='build dir for utils')
    args = parser.parse_args()

    if args.rundir is None:
        args.rundir = default_rundir(args.base)

    args.rundir = os.path.join(testnet_dir, args.rundir)

    if args.base == 'local':
        populate_local_filetree(args)
    else:
        print('--base=cloud not implemented yet')

if __name__ == '__main__':
    main()
