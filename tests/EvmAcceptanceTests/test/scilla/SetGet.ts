import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla SetGet contract", function () {
  let contract: ScillaContract;
  const VALUE = 12;
  const STRING_VALUE = "Salam";
  const ADDRESS_VALUE = "0x1cc45678901bbaa678cc12345ff8901234567890";

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
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

  it("Should set string state correctly", async function () {
    await contract.set_string(STRING_VALUE);
    expect(await contract.string_value()).to.be.eq(STRING_VALUE);
  });

  it("Should contain event data if get_string is called", async function () {
    const tx = await contract.get_string();
    expect(tx).to.have.eventLogWithParams("get_string", {value: STRING_VALUE, vname: "value"});
  });

  it("Should set address state correctly", async function () {
    await contract.set_address(ADDRESS_VALUE);
    expect(await contract.address_value()).to.be.eq(ADDRESS_VALUE);
  });

  it("Should contain event data if get_string is called", async function () {
    const tx = await contract.get_address();
    expect(tx).to.have.eventLogWithParams("get_address", {value: ADDRESS_VALUE, vname: "value"});
  });
});
