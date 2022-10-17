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

import pytest
from evm_ds_test import test_case


class TestBCInfo(test_case.EvmDsTestCase):

    @pytest.fixture(autouse=True)
    def _deploy_contract(self):
        self.init(num_accounts=1)
        compilation_result = self.compile_solidity_file("bcinfo.sol")
        self.contract = self.install_contract(self.account, compilation_result)

    def test_blockOrigin(self):
        self.init()
        resp = self.call_view(self.contract, "getOrigin")
        print(resp)

    def test_BlockCoinbase(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockCoinbase")
        print("getBlockCoinbase {}".format(resp))

    def test_BlockHash(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockHash", 0)
        print(resp)

    def test_BlockGasPrice(self):
        self.init()
        resp = self.call_view(self.contract, "getGasprice")
        print(resp)

    def test_BlockTimestamp(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockTimestamp")
        print(resp)

    def test_BlockGasLimit(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockGaslimit")
        print(resp)

    def test_BlockNumber(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockNumber")
        print(resp)

    def test_BlockDifficulty(self):
        self.init()
        resp = self.call_view(self.contract, "getBlockDifficulty")
        print(resp)
