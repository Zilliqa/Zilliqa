import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";
import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
const {expectRevert} = require("@openzeppelin/test-helpers"); // No declaration files found for oz-helpers

// FIXME: Can't be parallelized yet. Needs ZIL-5055
describe("Revert Contract Call", function () {
  let contract: Contract;
  let signer: SignerWithAddress;
  before(async function () {
    contract = await hre.deployContract("Revert");
    signer = contract.signer as SignerWithAddress;
  });

  it("Will revert the transaction when revert is called", async function () {
    await expect(contract.revertCall({value: 1000})).to.be.reverted;
  });

  it("Should revert transaction with a custom message if the called function reverts with custom message", async function () {
    const REVERT_MESSAGE = "reverted!!";
    await expect(contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000})).to.be.revertedWith(REVERT_MESSAGE);
  });

  it("Should revert transaction with a custom message in the JSONRPC return string @block-1", async function () {
    const REVERT_MESSAGE = "Really reverted!!";
    await expectRevert(contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000}), REVERT_MESSAGE);
  });

  it("Should revert with an error object if the called function reverts with custom error", async function () {
    const owner = signer;
    await expect(contract.revertCallWithCustomError({value: 1000}))
      .to.be.revertedWithCustomError(contract, "FakeError")
      .withArgs(1000, owner.address);
  });

  it("Should not be reverted despite its child possibly reverting", async function () {
    const owner = contract.signer;
    await expect(contract.callChainReverted()).not.to.be.reverted;
    await expect(contract.callChainOk()).not.to.be.reverted;
  });

  it("Should be reverted without any reason if specified gasLimit is not enough to complete txn", async function () {
    const txn = await contract.outOfGas({gasLimit: 100000});
    expect(txn).not.to.be.reverted;
    await expect(txn.wait()).eventually.to.be.rejected;
  });
});
