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
