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


class TestRevert(test_case.EvmDsTestCase):
    def test_revert_new_contract(self):
        self.init()
        compilation_result = self.compile_solidity(
            """
           // SPDX-License-Identifier: GPL-3.0
           pragma solidity >=0.7.0 <0.9.0;
           contract RevertOnInstall {
             string dummy;

             constructor() {
               dummy = "This is some dummy string, which will consume gas when set";
               revert();
             }
           }
        """
        )
        balance_before = self.get_balance(self.account.address)
        nonce_before = self.get_nonce(self.account.address)
        # with pytest.raises(APIError) as e:
        #     self.install_contract(self.account, compilation_result)
        # TODO: revert to checking raised exception when we can test failing contracts.
        try:
            self.install_contract(self.account, compilation_result, confirm=False)
        except ValueError:
            pass  # For the isolated server
        self.wait_txn_timeout()
        balance_after = self.get_balance(self.account.address)
        nonce_after = self.get_nonce(self.account.address)
        # Storing should consume at least 50 ZIL (gas is 0.002 ZIL)
        assert balance_before > balance_after
        # However, nonce should actually increase even after a revert, since
        # that transaction is actually confirmed by consensus, it just didn't
        # happen to succeed.
        assert nonce_after == nonce_before + 1

    def test_revert_on_call(self):
        self.init()
        compilation_result = self.compile_solidity(
            """
           // SPDX-License-Identifier: GPL-3.0
           pragma solidity >=0.7.0 <0.9.0;
           contract RevertOnCall {
             string public dummy = "hello revert";
             function revertMe () external {
               dummy = "This is some dummy string, which will consume gas when set";
               revert();
             }
           }
        """
        )
        contract = self.install_contract(self.account, compilation_result)
        balance_before = self.get_balance(self.account.address)
        # with pytest.raises(APIError) as e:
        #     self.call_contract(self.account, contract, 0, "revertMe")
        # TODO: revert to checking raised exception when we can test failing contracts.
        try:
            self.call_contract(self.account, contract, 0, "revertMe", confirm=False)
        except ValueError:
            pass  # For the isolated server
        self.wait_txn_timeout()
        balance_after = self.get_balance(self.account.address)
        assert balance_before > balance_after
        # Check that it indeed reverted.
        assert self.call_view(contract, "dummy") == "hello revert"
