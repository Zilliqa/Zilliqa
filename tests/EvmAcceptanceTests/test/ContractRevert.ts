import {expect} from "chai";
import {ethers} from "hardhat";
import {expectRevert} from "@openzeppelin/test-helpers";

// FIXME: Can't be parallelized yet. Needs ZIL-5055
describe("Revert Contract Call", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("Revert");
    this.contract = await Contract.deploy();
  });

  it("Will revert the transaction when revert is called", async function () {
    await expect(this.contract.revertCall({value: 1000})).to.be.reverted;
  });

  it("Should revert transaction with a custom message if the called function reverts with custom message", async function () {
    const REVERT_MESSAGE = "reverted!!";
    await expect(this.contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000})).to.be.revertedWith(REVERT_MESSAGE);
  });

  it("Should revert transaction with a custom message in the JSONRPC return string", async function () {
    const REVERT_MESSAGE = "Really reverted!!";
    await expectRevert(this.contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000}), REVERT_MESSAGE);
  });

  it("Should revert with an error object if the called function reverts with custom error", async function () {
    const owner = this.contract.signer;
    await expect(this.contract.revertCallWithCustomError({value: 1000}))
      .to.be.revertedWithCustomError(this.contract, "FakeError")
      .withArgs(1000, owner.address);
  });

  // FIXME: https://zilliqa-jira.atlassian.net/browse/ZIL-5001
  xit("Should be reverted without any reason if specified gasLimit is not enough to complete txn", async function () {
    await expect(this.contract.outOfGas({gasLimit: 100000})).to.be.revertedWithoutReason();
  });
});
