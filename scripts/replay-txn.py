# Invoke like `python replay-txn.py <txn id> <api URL of network to take tranasction from> <api URL of network to create transaction on>`
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


import sys
from typing import Optional
import requests
import hashlib
import curlify


# Adapted from https://github.com/deepgully/pyzil, licensed under the MIT License.
def hex_str_to_bytes(str_hex: str) -> bytes:
    """Convert hex string to bytes."""
    str_hex = str_hex.lower()
    if str_hex.startswith("0x"):
        str_hex = str_hex[2:]
    if len(str_hex) & 1:
        str_hex = "0" + str_hex
    return bytes.fromhex(str_hex)


# Adapted from https://github.com/deepgully/pyzil, licensed under the MIT License.
def bytes_to_int(bytes_hex: bytes, byteorder="big") -> int:
    """Convert bytes to int."""
    return int.from_bytes(bytes_hex, byteorder=byteorder)


# Adapted from https://github.com/deepgully/pyzil, licensed under the MIT License.
def hash256_bytes(*bytes_hex, encoding="utf-8") -> bytes:
    """Return hash256 digest bytes."""
    m = hashlib.sha256()
    for b in bytes_hex:
        if isinstance(b, str):
            b = b.encode(encoding=encoding)
        m.update(b)
    return m.digest()


# Adapted from https://github.com/deepgully/pyzil, licensed under the MIT License.
def to_checksum_address(address: str, prefix="0x") -> Optional[str]:
    """Convert address to checksum address."""
    address = address.lower().replace("0x", "")
    address_bytes = hex_str_to_bytes(address)
    v = bytes_to_int(hash256_bytes(address_bytes))

    checksum_address = prefix
    for i, c in enumerate(address):
        if not c.isdigit():
            if v & (1 << 255 - 6 * i):
                c = c.upper()
            else:
                c = c.lower()
        checksum_address += c

    return checksum_address


txn_id = sys.argv[1]
from_api = sys.argv[2]
to_api = sys.argv[3]
txn_id = txn_id.removeprefix("0x")
print(f"Replaying transaction {txn_id}")

response = requests.post(
    from_api,
    json={"jsonrpc": "2.0", "id": "1", "method": "GetTransaction", "params": [txn_id]},
)
txn = response.json()["result"]

create_request = {
    "version": int(txn["version"]),
    "nonce": int(txn["nonce"]),
    "toAddr": to_checksum_address(txn["toAddr"]),
    "amount": txn["amount"],
    "pubKey": txn["senderPubKey"].removeprefix("0x"),
    "gasPrice": txn["gasPrice"],
    "gasLimit": txn["gasLimit"],
    "code": txn["code"] if "code" in txn else None,
    "data": txn["data"] if "data" in txn else None,
    "signature": txn["signature"].removeprefix("0x"),
}
request = {
    "jsonrpc": "2.0",
    "id": "2",
    "method": "CreateTransaction",
    "params": [create_request],
}
response = requests.post(to_api, json=request)
print(curlify.to_curl(response.request))

response.raise_for_status()

print(response.json())
