import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import parallelizer from "../../helper/Parallelizer";
import { Zilliqa } from "@zilliqa-js/zilliqa";

// TODO: To be addressed in the next commit. They're not failing but needs playing with CI :-/
describe.skip("Scilla SetGet contract", function () {
  let contract: ScillaContract;
  const VALUE = 12;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("SetGet");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should set state correctly", async function () {
    await contract.set(VALUE);
    expect(await contract.value()).to.be.eq(VALUE);
  });

  it("Should contain event data if emit transition is called", async function () {
    const tx = await contract.emit();
    expect(tx).to.have.eventLog("Emit");
  });
});
