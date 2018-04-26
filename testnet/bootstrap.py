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

import os
import argparse
import datetime
import shutil
import subprocess
import random
import string
import stat

# working dir, it is assumed that bootstrap.py is in ROOT_DIR/testnet
WORKING_DIR = os.path.dirname(os.path.abspath(__file__))

# zilliqa project root dir
ROOT_DIR = os.path.dirname(WORKING_DIR)

# default path to genkeypair
GENKEYPAIR_BIN = os.path.join(ROOT_DIR, 'build', 'bin', 'genkeypair')

TESTNET_YAML = """# THIS FILE IS AUTO-GENERATED, DO NOT MODIFY
apiVersion: v1
kind: Service
metadata:
  name: {name}
  labels:
    testnet: {name}
spec:
  ports:
  - port: 30303
    name: zilliqa
  clusterIP: None
  selector:
    testnet: {name}
---
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: {name} # testnet id
spec:
  serviceName: {name}
  podManagementPolicy: Parallel
  replicas: {n}
  selector:
    matchLabels:
      testnet: {name}
  template:
    metadata:
      labels:
        testnet: {name}
    spec:
      containers:
      - name: zilliqa
        image: gnnng/zilliqa
        workingDir: /zilliqa-run
        command:
        - /bin/bash
        - -c
        - |
          set -xe
          # echo waiting network setup && sleep 90
          python /zilliqa-config/init.py
          # launch sequence
          chmod u+x /zilliqa-run/start.sh
          /zilliqa-run/start.sh
          #  tail -f /dev/null
        ports:
        - containerPort: 30303
          name: zilliqa
        volumeMounts:
        - name: zilliqa-run
          mountPath: /zilliqa-run
        - name: zilliqa-config
          mountPath: /zilliqa-config
      volumes:
      - name: zilliqa-config
        configMap:
          name: {name}-config
  volumeClaimTemplates:
  - metadata:
      name: zilliqa-run
    spec:
      accessModes: [ "ReadWriteOnce" ]
      resources:
        requests:
          storage: 1Gi
"""

INIT_PY = """#!/usr/bin/env python
# THIS FILE IS AUTO-GENERATED, DO NOT MODIFY
from __future__ import print_function

import subprocess
import shutil
import socket
import xml.etree.cElementTree as xtree
import netaddr
import struct
import time

n_all = {n}
n_ds = {d}
name = '{name}'

process = subprocess.Popen(['hostname'], stdout=subprocess.PIPE)
(output, err) = process.communicate()
exit_code = process.wait()

my_id = int(output.strip().split('-')[-1])

is_ds = my_id < n_ds

shutil.copyfile('/zilliqa-config/constants.xml', '/zilliqa-run/constants.xml')

# keep getting ip list until all DNS names are up
while True:
    ips = []
    try:
        for i in range(n_all):
            ips.append(socket.gethostbyname(name + '-' + str(i) + '.' + name))
    except:
        print('retrying resolving all DNS names')
        time.sleep(1)
        continue
    break

# get keys
keyfile = open('/zilliqa-config/keys.txt', 'r')
keypairs = keyfile.readlines()
keyfile.close()

# get my information
my_keypair = keypairs[my_id]
my_ip = ips[my_id]
my_pub_key, my_pri_key = my_keypair.strip().split(' ')

# generate config.xml if is_ds
if is_ds:
    nodes = xtree.Element("nodes")
    for i in range(n_ds):
        keypair = keypairs[i].strip().split(' ')
        peer = xtree.SubElement(nodes, "peer")
        xtree.SubElement(peer, "pubk").text = keypair[0]
        xtree.SubElement(peer, "ip").text = ips[i]
        xtree.SubElement(peer, "port").text = '30303'
    tree = xtree.ElementTree(nodes)
    tree.write("/zilliqa-run/config.xml")

# generate mykey.txt
with open('/zilliqa-run/mykey.txt', 'w') as mykeytxt:
    mykeytxt.write(my_keypair)

# generate start.sh
cmd_sendtxn = ' '.join([
    'sendtxn',
    '30303'
])

cmd_zilliqa = ' '.join([
    'zilliqa',
    my_pri_key,
    my_pub_key,
    my_ip,
    '30303',
    '1' if is_ds else '0', # if ds node
    '0',
    '0'
])

# hex string from IPv4 or IPv6 string
def ipHexStr(ip):
    ip = int(netaddr.IPAddress(ip))
    ip = '{{0:08X}}'.format(ip)
    ip = hex(struct.unpack('>I', struct.pack('<I', int(ip, 16)))[0])
    return '{{0:032X}}'.format(int(ip, 16))


block_num_0 = "0000000000000000000000000000000000000000000000000000000000000001"
diff = "0A" #genesis diff
rand1 = "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a" #genesis rand1
rand2 = "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf" #genesis rand2

ds_port = '{{0:08X}}'.format(30303)

cmd_setprimaryds = ' '.join([
    'sendcmd',
    '30303',
    'cmd',
    '0100' + ipHexStr(ips[0]) + ds_port
])

ds_network_info = ""
for x in range(0, n_ds):
    ds_ip = ipHexStr(ips[x])
    ds_network_info += my_pub_key + ds_ip + ds_port

cmd_startpow1 = ' '.join([
    'sendcmd',
    '30303',
    'cmd',
    '0200' + block_num_0 + diff + rand1 + rand2 + ds_network_info
])

def defer_cmd(cmd, seconds):
    return 'sleep {{}} && {{}} &'.format(seconds, cmd)

start_sh = [
    '#!/bin/bash',
    defer_cmd(cmd_setprimaryds, 10) if is_ds else '',
    defer_cmd(cmd_sendtxn, 20),
    defer_cmd(cmd_startpow1, 30) if not is_ds else '',
    cmd_zilliqa
]

with open('/zilliqa-run/start.sh', 'w') as startfile:
    for line in start_sh:
        startfile.write(line + '\\n')
"""

def setup_dir(args):
    print('Template dir:', args.template)
    print('Testnet name:', args.name)
    print('Running dir:', args.run_dir)

    template_dir = os.path.join(WORKING_DIR, args.template)
    run_dir = os.path.join(WORKING_DIR, args.run_dir)
    configmap_dir = os.path.join(WORKING_DIR, args.run_dir, 'configmap')

    def resource(filename, content):
        f_path = os.path.join(run_dir, filename)
        with open(f_path, 'w') as f:
            f.write(content)
        return f_path

    def configmap(filename, content):
        f_path = os.path.join(configmap_dir, filename)
        with open(f_path, 'w') as f:
            f.write(content)
        return f_path

    def script(filename, cmds):
        f_path = os.path.join(run_dir, filename)
        with open(f_path, 'w') as f:
            for cmd in cmds:
                f.write(cmd + '\n')
        st = os.stat(f_path)
        os.chmod(f_path, st.st_mode | stat.S_IEXEC)
        return f_path

    # pupoluate the rundir based on template dir
    try:
        shutil.copytree(template_dir, configmap_dir)
    except OSError as e:
        print(e)
        return

    # generate keypairs for all nodes
    keypairs = []
    for _ in range(args.n):
        process = subprocess.Popen([args.genkeypair], stdout=subprocess.PIPE)
        (output, err) = process.communicate()
        exit_code = process.wait()
        keypairs.append(output)
    keypairs.sort()

    # setup configmap folder
    print(configmap('name', args.name))

    print(configmap('keys.txt', ''.join(keypairs)))

    print(configmap('init.py', INIT_PY.format(n=args.n, d=args.d, name=args.name)))

    # setup k8s resources
    testnet_yaml = resource('testnet.yaml', TESTNET_YAML.format(name=args.name, n=args.n))
    print(testnet_yaml)

    # setup scripts
    commands = [
        '#!/bin/bash',
        'kubectl create configmap --from-file={} {}-config'.format(configmap_dir, args.name),
        'kubectl create -f {}'.format(testnet_yaml)
    ]
    print(script('start.sh', commands))

    commands = [
        '#!/bin/bash',
        'echo This script may take longer time to finish',
        'kubectl delete configmap {}-config'.format(args.name),
        'kubectl delete -f {}'.format(testnet_yaml)
    ]
    print(script('stop.sh', commands))

    commands = [
        '#!/bin/bash',
        'kubectl delete persistentvolumeclaim -l testnet={}'.format(args.name)
    ]
    print(script('delete.sh', commands))

    #  print('\ntear down with the following commands:\n')
    #  print('    kubectl delete configmap {}-config'.format(args.name))
    #  print('    kubectl delete -f {}'.format(os.path.join(run_dir, 'testnet.yaml')))
    #  print('    kubectl delete statefulset {}'.format(args.name))
    #  print('    kubectl delete service {}'.format(args.name))

    #  print('\nremove the persistent storages with:\n')
    #  print('TBD: kubectl delete')

def default_name():
    return 'testnet-'+ ''.join(random.choice(string.ascii_lowercase) for x in range(5))

def main():
    parser = argparse.ArgumentParser(description='bootstrap a local/cloud testnet')
    parser.add_argument('--genkeypair', default=GENKEYPAIR_BIN,
        help='binary path to genkeypair')
    parser.add_argument('-t', '--template', default='local',
        choices=['local', 'cloud'], help='template folder selection')
    parser.add_argument('-n', type=int, default=20,
        help='number of all nodes')
    parser.add_argument('-d', type=int, default=10,
        help='number of ds nodes')
    parser.add_argument('name', nargs='?', metavar='NAME', default=default_name(),
        help='optional testnet name (as unique id)')
    args = parser.parse_args()

    genkeypair = args.genkeypair
    if not (os.path.isfile(genkeypair) and os.access(genkeypair, os.X_OK)):
        print('genkeypair not available, see --genkeypair for information')
        return 1

    args.run_dir = args.template + '_' + args.name
    setup_dir(args)

if __name__ == '__main__':
    main()
