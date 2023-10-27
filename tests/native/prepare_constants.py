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
import shutil
import sys
from xml.etree import cElementTree as ET
def update_xml_files(source_file, target_file):

    root = ET.parse(source_file).getroot()
    if not root:
        print("Failed to parse xml file")
        os.abort()

    DEV_TREE_ROOT=os.environ.get('DEV_TREE_ROOT')

    if not DEV_TREE_ROOT:
        print("DEV_TREE_ROOT is not set")
        os.abort()

    general = root.find('general')
    if general:
        general.find('LOOKUP_NODE_MODE').text = 'false'
        general.find('DEBUG_LEVEL').text = '4'

    scilla_root = root.find('smart_contract')
    if scilla_root:
        scilla_root.find('SCILLA_ROOT').text = DEV_TREE_ROOT +'/scilla'
        scilla_root.find('ENABLE_SCILLA_MULTI_VERSION').text = 'false'

    jsonrpc = root.find('jsonrpc')
    if jsonrpc:
        jsonrpc.find('EVM_SERVER_BINARY').text = DEV_TREE_ROOT + '/Zilliqa/evm-ds/target/release/evm-ds'
        jsonrpc.find('ENABLE_STATUS_RPC').text = 'false'

    metric = root.find('metric/zilliqa')
    if metric:
        metric.find('METRIC_ZILLIQA_PROVIDER').text = 'NONE'
        metric.find('METRIC_ZILLIQA_MASK').text = 'NONE'

    trace = root.find('trace/zilliqa')
    if trace:
        trace.find('TRACE_ZILLIQA_PROVIDER').text = 'NONE'
        trace.find('TRACE_ZILLIQA_MASK').text = 'NONE'

    logging = root.find('logging/zilliqa')
    if logging:
        logging.find('LOGGING_ZILLIQA_PROVIDER').text = 'NONE'

    tree = ET.ElementTree(root)
    tree.write(target_file)

def main():
    numargs = len(sys.argv)
    if (numargs < 2):
        os.abort()
    else:
        in_file = sys.argv[1]
        out_file = sys.argv[2]

    update_xml_files(in_file, out_file)

if __name__ == '__main__':
    main()