#!/usr/bin/env python3
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

from typing import List
from pyzil.crypto import zilkey
from natlib.ip import get_real_ip


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


class Data:

    IP = 0
    PORT = 1
    PUBLIC = 2

    def __init__(self) -> object:
        self.normal_ips_from_origin = []
        self.lookup_ips_from_origin = []
        self.multiplier_ips_from_origin = []
        self.seedpub_ips_from_origin = []
        self.guard_ips_from_origin = []
        self.origin_server = ""

    def get_normal(self):
        return self.normal_ips_from_origin

    def get_lookup(self) -> List[object]:
        return self.lookup_ips_from_origin

    def get_multiplier(self):
        return self.multiplier_ips_from_origin

    def get_seedpub(self):
        return self.seedpub_ips_from_origin

    def get_guard(self):
        return self.guard_ips_from_origin

    def __str__(self):
        return f"normal: {self.normal_ips_from_origin}\nlookup: {self.lookup_ips_from_origin}\nmultiplier: {self.multiplier_ips_from_origin}\nSeedpub:{self.seedpub_ips_from_origin}\nGaurd:{self.gaurd_ips_from_origin}\n"

    def genkey(self, num) -> List[str]:
        keypairs = []
        for _ in range(num):
            key = zilkey.ZilKey.generate_new()
            publicKey, privateKey = key.keypair_str
            keypairs.append(' '.join([publicKey.upper(), privateKey.upper()]))
        keypairs.sort()
        return keypairs

    def print_details(self):
        print("\n\nIP,Port and address Allocations")
        print("normal")
        for line in self.normal_ips_from_origin:
            print(line)
        print("lookup")
        for line in self.lookup_ips_from_origin:
            print(line)
        print("multiplier")
        for line in self.multiplier_ips_from_origin:
            print(line)
        print("seedpub")
        for line in self.seedpub_ips_from_origin:
            print(line)
        print("guard")
        for line in self.guard_ips_from_origin:
            print(line)
        print("")

    def get_ips_list_from_pseudo_origin(self, arg) -> bool:
        try:
            ''' Allocate the ports sequentitally '''
            self.normal_port = arg.port
            self.guard_port = self.normal_port + (((arg.n - arg.d)*5) + 1)
            self.lookup_port = self.guard_port + (arg.ds_guard*5) + 1
            self.seedpub_port = self.lookup_port + (arg.l*5) + 1
            self.multiplier_port = self.seedpub_port + (sum(arg.multiplier_fanout)*5) + 1
            self.nextfreeport = self.multiplier_port + (len(arg.multiplier_fanout)*5) + 1
            self.my_ip = get_real_ip()
            self.my_ip = "127.0.0.1"

        except Exception as _:
            print("We suspect the arguments are suspect")
            return False
        try:
            # lookup

            self.lookup_ips_from_origin = list(
                zip([str(self.my_ip)] * arg.l, range(self.lookup_port, self.lookup_port + arg.l, 5),
                    self.genkey(arg.l)))
            # seedpub
            self.seedpub_ips_from_origin = list(zip([str(self.my_ip)] * sum(arg.multiplier_fanout),
                                                    range(self.seedpub_port,
                                                          self.seedpub_port + sum(arg.multiplier_fanout), 5),
                                                    self.genkey(sum(arg.multiplier_fanout))))
            # multiplier
            self.multiplier_ips_from_origin = list(zip([str(self.my_ip)] * len(arg.multiplier_fanout),
                                                       range(self.multiplier_port,
                                                             self.multiplier_port + len(arg.multiplier_fanout), 5),
                                                       self.genkey(len(arg.multiplier_fanout))))

            # now normals

            keypairs = self.genkey(arg.n)

            self.guard_ips_from_origin = list(zip([str(self.my_ip)] * arg.ds_guard,
                                                  range(self.guard_port,
                                                        self.guard_port + (5 * arg.ds_guard), 5),
                                                  keypairs[0:arg.ds_guard]))

            keys = keypairs[arg.d:arg.n]
            ports = range(self.normal_port, self.normal_port + arg.n - arg.d, 5)
            ips = [str(self.my_ip)] * (arg.n - arg.ds_guard)
            self.normal_ips_from_origin = list(zip(ips, ports, keys))

        except Exception as _:
            print("We suspect the arguments are suspect")
            return False

        return True
