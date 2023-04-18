import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla giant contract", function () {
  let contract: ScillaContract;

  before(async function() {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("GiantStateTest");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should be possible to add values", async function() {
     const tx = await contract.AddState(0, 10);
     console.log(`${JSON.stringify(tx)}`);
   });

   it("Should be possible to add lots of values", async function() {
     const tx = await contract.AddState(1000, 2000);
   }).timeout(180_000);

  it("Should be possible to add lots more values", async function() {
    let tx = await contract.AddState(6000, 9000);
  }).timeout(300_000);

  it("Should be possible to run out of gas", async function() {
    let tx = await contract.AddState(10000, 18000);
  }).timeout(300_000);
});
