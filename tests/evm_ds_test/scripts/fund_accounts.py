#
# Copyright (C) 2022 Zilliqa
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

from pyzil.account import Account
from pyzil.zilliqa.chain import BlockChain, set_active_chain, active_chain
from pyzil.crypto.zilkey import to_checksum_address, ZilKey


def fund(private_key, destination, amount):
    acc = Account(private_key=private_key)
    blockchain = BlockChain(
        api_url="https://evmdev-l2api.dev.z7a.xyz",
        version=int("0x%04x%04x" % (333, 1), 0),
        network_id=333,
    )
    set_active_chain(blockchain)
    print("Current balance: ", acc.get_balance())
    nonce = acc.get_nonce() + 1
    if amount > 0:
        acc.transfer(
            to_addr=to_checksum_address(destination),
            zils=amount,
            gas_limit=100,
            priority=True,
            nonce=nonce,
            confirm=True,
        )


if __name__ == "__main__":
    import sys

    fund(sys.argv[1], sys.argv[2], int(sys.argv[3], 0))
