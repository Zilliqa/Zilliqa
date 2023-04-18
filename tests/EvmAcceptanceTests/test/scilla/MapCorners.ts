import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("MapCorners contract", function () {
  let contract: ScillaContract;

  before(async function() {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("MapCornersTest");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should run tests", async function() {
    const tx = await contract.test();
    console.log(`${JSON.stringify(tx)}`)
  });

  it("Should be able to spam tests", async function() {
    var promises = [];
    for (let i =0 ;i < 10; ++i) {
      promises.push(contract.test());
    }
    const results = await Promise.all(promises);
    console.log(`${JSON.stringify(results)}`);
  });
  
    
});

