import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla ByStr Functionality", function () {
  let contract: ScillaContract;
  let BYSTR5_VALUE = "0x1234567890";
  let BYSTR6_VALUE = "0x223344556677";

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("ByStrFunctionality");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("should return ByStr5 field", async function () {
    const tx = await contract.getByStr5Field();
    expect(tx).to.have.eventLogWithParams("getByStr5()", {value: BYSTR5_VALUE, vname: "value"});
  });

  it("should return expected value if builtin concat is called", async function () {
    const tx = await contract.getByStrConcat();
    expect(tx).to.have.eventLogWithParams("getByStrConcat()", {
      value: BYSTR5_VALUE + BYSTR6_VALUE.substring(2),
      vname: "value"
    });
  });

  it("should return expected value if builtin strlen is called", async function () {
    const tx = await contract.getByStrLen("0x112233");
    expect(await contract.bystr_len()).to.be.eq(3);
  });
});
