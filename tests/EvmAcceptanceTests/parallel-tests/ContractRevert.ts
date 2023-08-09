import {Block, Scenario, scenario, xit, it} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";
import {expectRevert} from "@openzeppelin/test-helpers";

export const contractRevertScenario = function(contract: Contract): Scenario {
  const REVERT_MESSAGE = "reverted!";

  return scenario("Revert",
    it("Will revert the transaction when revert is called",
      async() => await expect(contract.revertCall({value: 1000})).to.be.reverted,
      Block.BLOCK_1
    ),
    it("Should revert transaction with a custom message if the called function reverts with custom message",
      async() => {
          await expect(contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000})).to.be.revertedWith(REVERT_MESSAGE);
      },
      Block.BLOCK_1
    ),
    it("Should revert transaction with a custom message in the JSONRPC return string", async function () {
      const REVERT_MESSAGE = "Really reverted!!";
      await expectRevert(contract.revertCallWithMessage(REVERT_MESSAGE, { value: 1000 }), REVERT_MESSAGE);
    },
      Block.BLOCK_1
    ),
    it("Should revert with an error object if the called function reverts with custom error", async function () {
      const owner = contract.signer;
      await expect(contract.revertCallWithCustomError({value: 1000}))
        .to.be.revertedWithCustomError(contract, "FakeError")
        .withArgs(1000, await owner.getAddress());
    },
      Block.BLOCK_1
    ),

    xit("Should not be reverted despite its child possibly reverting", async function () {
      await expect(contract.callChainReverted()).not.to.be.reverted;
      await expect(contract.callChainOk()).not.to.be.reverted;
    },
      Block.BLOCK_1
    ),

    xit("Should be reverted without any reason if specified gasLimit is not enough to complete txn", async function () {
      const txn = await contract.outOfGas({ gasLimit: 100000 });
      expect(txn).not.to.be.reverted;
      await expect(txn.wait()).eventually.to.be.rejected;
    },
      Block.BLOCK_1
    ),
  )
}
