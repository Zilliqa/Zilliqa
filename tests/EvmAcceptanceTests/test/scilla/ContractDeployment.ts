import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla Contract Deployment", function () {
  context("String immutable variable", function () {
    let contract: ScillaContract;
    before(async function () {
      if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
        this.skip();
      }

      contract = await parallelizer.deployScillaContract("ImmutableString", "TEST");
    });

    it("Should be deployed successfully", async function () {
      expect(contract.address).to.be.properAddress;
    });

    it("Should be possible to get initial string passed as an argument to the contract", async function () {
      const tx = await contract.getString();
      expect(tx).to.have.eventLogWithParams("getString()", {value: "TEST", vname: "msg", type: "String"});
    });

    it("Should be possible to get contract address using implicit _this_address variable", async function () {
      const tx = await contract.getContractAddress();
      expect(tx).to.have.eventLogWithParams("getContractAddress()", {
        value: contract.address?.toLowerCase(),
        vname: "address",
        type: "ByStr20"
      });
    });
  });
});
