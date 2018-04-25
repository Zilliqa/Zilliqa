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

# current dir
testnet_dir = os.path.dirname(os.path.abspath(__file__))

# zilliqa project root dir
root_dir = os.path.dirname(testnet_dir)

# default path to genkeypair
gen_bin = os.path.join(root_dir, 'build', 'bin', 'genkeypair')

k8s_yaml = """# THIS FILE IS AUTO-GENERATED, DO NOT MODIFY
apiVersion: v1
kind: Service
metadata:
  name: {name}
  labels:
    app: zilliqa
spec:
  ports:
  - port: 30303
    name: zilliqa
  clusterIP: None
  selector:
    app: zilliqa
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
      app: zilliqa
  template:
    metadata:
      labels:
        app: zilliqa
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
          echo waiting network setup && sleep 90
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

init_py="""#!/usr/bin/env python
# THIS FILE IS AUTO-GENERATED, DO NOT MODIFY
from __future__ import print_function

import subprocess
import shutil
import socket
import xml.etree.cElementTree as xtree
import os
import netaddr
import struct

n_all = {n}
n_ds = {d}
name = '{name}'

process = subprocess.Popen(['hostname'], stdout=subprocess.PIPE)
(output, err) = process.communicate()
exit_code = process.wait()

id = int(output.strip().split('-')[-1])

is_ds = id < n_ds

shutil.copyfile('/zilliqa-config/constants.xml', '/zilliqa-run/constants.xml')

# get ip list
ips = []
for i in range(n_all):
    ips.append(socket.gethostbyname(name + '-' + str(i) + '.' + name))

# get keys
keyfile = open('/zilliqa-config/keys.txt', 'r')
keypairs = keyfile.readlines()
keyfile.close()

# get my information
my_keypair = keypairs[id]
my_ip = ips[id]
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

block_num_0 = "0000000000000000000000000000000000000000000000000000000000000001"
diff = "0A" #genesis diff
rand1 = "2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a" #genesis rand1
rand2 = "e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf" #genesis rand2

ds_network_info = ""
ds_port = '{{0:08X}}'.format(30303)
for x in range(0, n_ds):
    ds_ip = int(netaddr.IPAddress(ips[x]))
    ds_ip = '{{0:08X}}'.format(ds_ip)
    ds_ip = hex(struct.unpack('>I',struct.pack('<I',int(ds_ip, 16)))[0])
    ds_ip = '{{0:032X}}'.format(int(ds_ip,16))
    ds_network_info += my_pub_key + ds_ip + ds_port

startpow1_hex = '0200' + block_num_0 + diff + rand1 + rand2 + ds_network_info

cmd_startpow1 = ' '.join([
    'sendcmd',
    '30303',
    'cmd',
    startpow1_hex
])

start_sh = [
    '#!/bin/bash',
    'sleep 10 && ' + cmd_sendtxn + ' &',
    'sleep 20 && ' + cmd_startpow1 + ' &' if not is_ds else '',
    cmd_zilliqa
]

with open('/zilliqa-run/start.sh', 'w') as startfile:
    for line in start_sh:
        startfile.write(line + '\\n')
"""

def setup_dir(args):
    template_dir = args.template
    run_dir = os.path.relpath(args.rundir, testnet_dir)

    print('Testnet name:', args.name)
    print('Template dir:', template_dir)
    print('Running dir:', run_dir)

    # copy all template files from template dir
    try:
        shutil.copytree(template_dir, args.rundir)
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

    with open(os.path.join(args.rundir, 'keys.txt'), "w") as keyfile:
        keyfile.writelines(keypairs)
    print(len(keypairs), 'keyfile generated: keys.txt')

    # generate k8s statefulset yaml file
    with open(os.path.join(args.rundir, 'testnet.yaml'), 'w') as k8sfile:
        k8sfile.write(k8s_yaml.format(name=args.name, n=args.n))
    print('kubernete file generated: testnet.yaml')

    with open(os.path.join(args.rundir, 'name'), 'w') as namefile:
        namefile.write(args.name)

    with open(os.path.join(args.rundir, 'init.py'), 'w') as initpyfile:
        initpyfile.write(init_py.format(n=args.n, d=args.d, name=args.name))

    print('\nbring up with the following commands:\n')
    print('    kubectl create configmap --from-file={} {}-config'.format(run_dir, args.name))
    print('    kubectl create -f {}'.format(os.path.join(run_dir, 'testnet.yaml')))

    print('\ntear down with the following commands:\n')
    print('    kubectl delete configmap {}-config'.format(args.name))
    print('    kubectl delete -f {}'.format(os.path.join(run_dir, 'testnet.yaml')))
    #  print('    kubectl delete statefulset {}'.format(args.name))
    #  print('    kubectl delete service {}'.format(args.name))

    print('remove the persistent storages with:\n')
    print('TBD: kubectl delete')

def timestamp():
    return datetime.datetime.now().strftime('%Y-%m-%d_%H%M%S')

def default_name():
    return 'testnet-'+ ''.join(random.choice(string.ascii_lowercase) for x in range(5))

def main():
    parser = argparse.ArgumentParser(description='bootstrap a local/cloud testnet')
    parser.add_argument('--genkeypair', default=gen_bin,
        help='binary path for genkeypair')
    parser.add_argument('-t', '--template', default='local',
        choices=['local', 'cloud'], help='template folder selection')
    parser.add_argument('-r', '--rundir', metavar='PATH',
        help='the root dir for storing config')
    parser.add_argument('-n', type=int, default=20,
        help='testnet total nodes')
    parser.add_argument('-d', type=int, default=10,
        help='number of ds nodes')
    parser.add_argument('name', nargs='?', metavar='NAME', default=default_name(),
        help='testnet name (as unique id)')
    args = parser.parse_args()

    genkeypair = args.genkeypair
    if not (os.path.isfile(genkeypair) and os.access(genkeypair, os.X_OK)):
        print('genkeypair not available, see --genkeypair for information')
        return 1

    if args.rundir is None:
        args.rundir = args.template + '_' + args.name
        #  args.rundir = args.template + '_' + args.name + '_' + timestamp()

    args.rundir = os.path.join(testnet_dir, args.rundir)

    if os.path.isdir(args.rundir):
        print(args.rundir, 'existed, terminated')
        return 1

    setup_dir(args)

if __name__ == '__main__':
    main()
