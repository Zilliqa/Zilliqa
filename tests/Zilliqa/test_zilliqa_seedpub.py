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

import os
import sys
import shutil
import stat

from subprocess import Popen, PIPE
import xml.etree.cElementTree as ET


NODE_LISTEN_PORT = 4201

LOCAL_RUN_FOLDER = './seedpub_local_run/'
LOCAL_FOLDER = "./"

GENTXN_WORKING_DIR = os.path.join(LOCAL_RUN_FOLDER, 'gentxn')

# need to be an absolute path
TXN_PATH = "/tmp/zilliqa_txns"


def print_usage():
    print("Testing multiple Zilliqa nodes in local machine\n"
          "===============================================\n"
          "Usage:\n\tpython " + sys.argv[0] + " [command] [command parameters]\n"
                                              "Available commands:\n"
                                              "\tTest Execution:\n"
                                              "\t\tsetup [num-nodes]           - Set up the nodes\n"
                                              "\t\tstart                       - Start node processes\n"
                                              "\t\tgentxn [seconds]            - generate transactions\n")


def main():
    numargs = len(sys.argv)
    if numargs < 2:
        print_usage()
    else:
        command = sys.argv[1]
        if command == 'setup':
            print_usage() if (numargs != 3) else run_setup(numnodes=int(sys.argv[2]), printnodes=True)
        elif command == 'start':
            print_usage() if (numargs != 2) else run_start()
        elif command == 'gentxn':
            print_usage() if (numargs != 3) else run_gentxn(batch=int(sys.argv[2]))
        else:
            print_usage()


# ================
# Helper Functions
# ================

def get_immediate_subdirectories(a_dir):
    subdirs = [name for name in os.listdir(a_dir) if
               os.path.isdir(os.path.join(a_dir, name)) and name.startswith('node')]
    subdirs.sort()
    return subdirs


# ========================
# Test Execution Functions
# ========================

def run_setup(numnodes, printnodes):
    os.system('killall seedpub')
    if os.path.exists(LOCAL_RUN_FOLDER) is not True:
        # shutil.rmtree(LOCAL_RUN_FOLDER)
        os.makedirs(LOCAL_RUN_FOLDER)
    for x in range(0, numnodes):
        testsubdir = LOCAL_RUN_FOLDER + 'node_' + str(x + 1).zfill(4)
        if os.path.exists(testsubdir) is not True:
            os.makedirs(testsubdir)
        shutil.copyfile('./bin/zilliqa', testsubdir + '/seedpub')

        st = os.stat(testsubdir + '/seedpub')
        os.chmod(testsubdir + '/seedpub', st.st_mode | stat.S_IEXEC)

    testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
    count = len(testfolders_list)

    if printnodes:
        for x in range(0, count):
            print('[Node ' + str(x + 1).ljust(3) + '] [Port ' + str(NODE_LISTEN_PORT + x) + '] ' + LOCAL_RUN_FOLDER +
                  testfolders_list[x])

    keypairs = []
    # Generate keypairs (sort by public key)
    for x in range(0, count):
        process = Popen(["./bin/genkeypair"], stdout=PIPE, universal_newlines=True)
        (output, err) = process.communicate()
        exit_code = process.wait()
        keypairs.append(output.strip())
    keypairs.sort()


    nodes = ET.Element("nodes")

    # Store sorted keys list in text file
    keys_file = open(LOCAL_RUN_FOLDER + 'seedpub_keys.txt', "w")
    for x in range(0, count):
        keys_file.write(keypairs[x] + '\n')
        keypair = keypairs[x].split(" ")
        if x < count:
            peer = ET.SubElement(nodes, "peer")
            ET.SubElement(peer, "pubk").text = keypair[0]
            ET.SubElement(peer, "ip").text = '127.0.0.1'
            ET.SubElement(peer, "port").text = str(NODE_LISTEN_PORT + x)
    keys_file.close()


def patch_constants_xml(filepath, read_txn=False):
    return

def run_gentxn(batch=100):
    if not os.path.exists(TXN_PATH):
        os.makedirs(TXN_PATH)

    if not os.path.exists(GENTXN_WORKING_DIR):
        os.makedirs(GENTXN_WORKING_DIR)

    print("Created gentxn working folder: " + GENTXN_WORKING_DIR)
    shutil.copy('bin/gentxn', os.path.join(GENTXN_WORKING_DIR, 'gentxn'))
    gentxn_constants_xml_path = os.path.join(GENTXN_WORKING_DIR, 'constants.xml')
    shutil.copyfile('constants_local.xml', gentxn_constants_xml_path)

    if os.path.exists(gentxn_constants_xml_path):
        patch_constants_xml(gentxn_constants_xml_path)

    print("Waiting gentxn for creating {} batches".format(batch))
    os.system('cd ' + GENTXN_WORKING_DIR + '; ./gentxn --begin 0 --end {}'.format(batch))


def patch_lookup_pubkey(filepath, keypairs, count):
    root = ET.parse(filepath).getroot()
    td = root.find('lookups')
    elems = td.findall('peer/pubkey')
    for x in range(0, count):
        keypair = keypairs[x].split(" ")
        elems[x].text = keypair[0]
    tree = ET.ElementTree(root)
    tree.write(filepath)


def patch_seed_pubkey(filepath, keypairs, count):
    root = ET.parse(filepath).getroot()
    td = root.find('upper_seed')
    elems = td.findall('peer/pubkey')
    for x in range(0, count):
        keypair = keypairs[x].split(" ")
        elems[x].text = keypair[0]
    tree = ET.ElementTree(root)
    tree.write(filepath)


def run_start():
    testfolders_list = get_immediate_subdirectories(LOCAL_RUN_FOLDER)
    count = len(testfolders_list)
    dev_root = os.getenv("DEV_TREE_ROOT")
    if dev_root is None:
        print("DEV_TREE_ROOT is not set")
        return

    dev_root += "/Zilliqa"
    fp = dev_root + "/" + "constants_local.xml.native"

    if not os.path.exists(fp):
        print( fp +" not found")
        return



    # Load the keypairs
    keypairs = []
    with open(LOCAL_RUN_FOLDER + 'seedpub_keys.txt', "r") as f:
        keypairs = f.readlines()
    keypairs = [x.strip() for x in keypairs]

    for x in range(0, count):
        shutil.copyfile('config_normal.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/config.xml')
        shutil.copyfile(fp, LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml')
        shutil.copyfile('dsnodes.xml', LOCAL_RUN_FOLDER + testfolders_list[x] + '/dsnodes.xml')
        # FIXME: every lookup node has the option USE_REMOTE_TXN_CREATOR set to true, which seemingly
        # enable transaction dispatching on every lookup running locally. However, the truth is only the
        # one with the jsonrpc server running will do the transaction dispatching and coincidentally there
        # will be only one as there is an unknown issue that multiple lookup nodes are having port collision
        # on 4201 and eventually only one will get the port and others won't be able to start jsonrpc server
        patch_constants_xml(LOCAL_RUN_FOLDER + testfolders_list[x] + '/constants.xml', True)

    # Launch node zilliqa process
    for x in range(0, count):
        keypair = keypairs[x].split(" ")
        os.system('cd ' + LOCAL_RUN_FOLDER + testfolders_list[x] + '; echo \"' + keypair[0] + ' ' + keypair[
            1] + '\" > mykey.txt' + '; ulimit -n 65535; ulimit -Sc unlimited; ulimit -Hc unlimited; $(pwd)/seedpub ' +
                  ' --privk ' + keypair[1] + ' --pubk ' + keypair[0] + ' --address ' + '127.0.0.1' + ' --port ' +
                  str(NODE_LISTEN_PORT + x) + ' --identity ' + 'seedpub-' + str(x) + ' > ./error_log_zilliqa 2>&1 &')


if __name__ == "__main__":
    main()
