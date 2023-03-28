import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla HelloWorld contract", function () {
  let contract: ScillaContract;
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("FungibleToken", parallelizer.zilliqaAccountAddress, "Saeed's Token", "SDT", 2, 1_000);
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should have Saeed's Token at its name", async function () {
    expect(await contract.name()).to.be.eq("Saeed's Token")
  });
});
