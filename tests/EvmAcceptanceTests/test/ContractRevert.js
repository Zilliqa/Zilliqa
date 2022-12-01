const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Revert Contract Call", function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("Revert");
    contract = await Contract.deploy();
  });

  it("Will revert the transaction when revert is called", async function () {
    await expect(contract.revertCall({value: 1000})).to.be.reverted;
  });

  it("Should revert transaction with a custom message if the called function reverts with custom message", async function () {
    const REVERT_MESSAGE = "reverted!!";
    await expect(contract.revertCallWithMessage(REVERT_MESSAGE, {value: 1000})).to.be.revertedWith(REVERT_MESSAGE);
  });

  it("Should revert with an error object if the called function reverts with custom error", async function () {
    const [owner] = await ethers.getSigners();
    await expect(contract.revertCallWithCustomError({value: 1000}))
      .to.be.revertedWithCustomError(contract, "FakeError")
      .withArgs(1000, owner.address);
  });

  // FIXME: https://zilliqa-jira.atlassian.net/browse/ZIL-5001
  it("Should be reverted without any reason if specified gasLimit is not enough  to complete txn", async function () {
    await expect(contract.outOfGas({gasLimit: 100000})).to.be.revertedWithoutReason();
  });
});
