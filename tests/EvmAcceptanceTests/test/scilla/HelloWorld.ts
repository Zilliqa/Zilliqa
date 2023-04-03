import {expect} from "chai";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("Scilla HelloWorld contract", function () {
  let contract: ScillaContract;
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("HelloWorld", parallelizer.zilliqaAccountAddress);
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Can be possible to call setHello by the owner", async function () {
    const tx = await contract.setHello("salam");
    expect(tx).to.eventLogWithParams("setHello()", {value: "2", vname: "code"});
    expect(await contract.welcome_msg()).to.be.eq("salam");
  });

  it("Should send getHello() event when getHello() transition is called", async function () {
    const tx = await contract.getHello();
    expect(tx).to.have.eventLogWithParams("getHello()", {value: "salam", vname: "msg", type: "String"});
  });
});
