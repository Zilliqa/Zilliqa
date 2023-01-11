#!/usr/bin/env python


import xml.etree.cElementTree as xtree
import argparse

def create_gentxn_constants_xml(target_path, source_path, genesis_file):
    try:
        genesis_root = xtree.parse(genesis_file).getroot()
        root = xtree.parse(source_path).getroot()

        # Rebuild the accounts element
        root.remove(root.find('accounts'))
        accounts = xtree.SubElement(root, 'accounts')

        # add default genesis accounts
        accounts.extend(genesis_root.findall('.accounts/account'))

        tree = xtree.ElementTree(root)
        tree.write(target_path)
    except Exception as e:
        print("Exception when creating gentxn_constants.xml: {}".format(e))
        return False
    return True

def main():
    parser = argparse.ArgumentParser(description='initialize gentxn')

    parser.add_argument('--constants-xml', metavar='PATH', default='/etc/zilliqa/constants.xml', help='The path to the file constants.xml')
    parser.add_argument('--genesis-xml', metavar='PATH', default='/etc/zilliqa/genesis.xml', help='The path to the file genesis.xml')

    args = parser.parse_args()

    if create_gentxn_constants_xml("constants.xml", args.constants_xml, args.genesis_xml):
        print("constants.xml created successfully")
    else:
        print("constants.xml was not created")

if __name__ == "__main__":
    main()
