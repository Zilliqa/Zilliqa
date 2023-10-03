import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {Account} from "@zilliqa-js/zilliqa";

describe("Scilla ByStr Functionality #parallel", function () {
  let contract: ScillaContract;
  let signer: Account;
  let BYSTR5_VALUE = "0x1234567890";
  let BYSTR6_VALUE = "0x223344556677";

  before(async function () {
    signer = hre.allocateZilSigner();
    contract = await hre.deployScillaContractWithSigner("ByStrFunctionality", signer);
  });

  it("Should be deployed successfully @block-1", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("should return ByStr5 field @block-1", async function () {
    const tx = await contract.getByStr5Field();
    expect(tx).to.have.eventLogWithParams("getByStr5()", {value: BYSTR5_VALUE, vname: "value"});
  });

  it("should return expected value if builtin concat is called @block-2", async function () {
    const tx = await contract.getByStrConcat();
    expect(tx).to.have.eventLogWithParams("getByStrConcat()", {
      value: BYSTR5_VALUE + BYSTR6_VALUE.substring(2),
      vname: "value"
    });
  });

  it("should return expected value if builtin strlen is called @block-3", async function () {
    const tx = await contract.getByStrLen("0x112233");
    expect(await contract.bystr_len()).to.be.eq(3);
  });
});
