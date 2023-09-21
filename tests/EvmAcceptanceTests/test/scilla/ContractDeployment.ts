import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre, {ethers} from "hardhat";
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

    it("Should be possible to the get contract address using `GetContractAddressFromTransactionID`", async function () {
      let address = (
        await parallelizer.zilliqaSetup.zilliqa.blockchain.getContractAddressFromTransactionID(contract.deployed_by.id)
      ).result;
      expect(address).to.be.equal(contract.address!!.toLowerCase().replace("0x", ""));
    });

    it("Should be possible to the get contract address using `eth_getTransactionReceipt`", async function () {
      let address = (await ethers.provider.getTransactionReceipt(`0x${contract.deployed_by.id}`)).contractAddress;
      expect(address.toLowerCase()).to.be.equal(contract.address!!.toLowerCase());
    });
  });
});
