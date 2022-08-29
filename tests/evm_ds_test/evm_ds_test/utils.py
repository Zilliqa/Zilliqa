"""
Get Int from a big endian sequence of bytes encoded as string.
"""
from eth_typing import HexStr
from hexbytes import HexBytes


def int_from_bytes(arg):
    return int.from_bytes(arg, byteorder="big")


def string_from_bytes(arg):
    """
    Get String from a sequence of bytes
    """
    # The string saved has \x10 as the last character
    return arg.rstrip(b"\x10").rstrip(b"\x00").decode("utf-8")


def process_data(arg):
    return bytearray(arg).hex()


def process_log(arg, index, id, epoch_num):
    ret = {}
    ret["logIndex"] = index
    ret["transactionIndex"] = 0
    ret["transactionHash"] = id
    ret["blockHash"] = HexBytes(
        "0x9cc1a28ef6a5512147394d383b8ed456d94afee1abd7c89e818759039bd5e004"
    )  # dummy value
    ret["blockNumber"] = epoch_num
    ret["topics"] = []
    for i in arg["topics"]:
        ret["topics"].append(HexBytes(i))

    ret["data"] = HexStr(bytearray(arg["data"]).hex())
    ret["address"] = arg["address"]

    return ret


def from_zil(zil):
    """Returns Zil converted to QA"""
    return zil * 1_000_000_000_000
