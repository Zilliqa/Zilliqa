import {expect} from "chai";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";
import hre, {ethers} from "hardhat";

describe("Scilla timestamp", () => {
  let contract: ScillaContract;
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("Timestamp");
  });

  it("Should send back timestamp of a block", async () => {
    const blockCount = await ethers.provider.getBlockNumber();
    const blockTimestamp = (await ethers.provider.getBlock(blockCount)).timestamp;
    const tx = await contract.EventTimestamp(blockCount);
    const timestamp = Number(tx.getReceipt().event_logs[0].params[0].value.arguments[0]);
    expect(Math.floor(timestamp / 1000_000)).to.be.eq(blockTimestamp);
  });
});
