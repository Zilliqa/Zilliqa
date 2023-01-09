import {deploy, sc_call} from "../../helper/ScillaHelper";
import {Contract} from "@zilliqa-js/contract";
import {expect} from "chai";

describe("Scilla SetGet contract", function () {
  let contract: Contract;
  before(async function () {
    contract = await deploy("SetGet");
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should set state correctly", async function () {
    const VALUE = 12;
    const args = [{vname: "v", type: "Uint128", value: VALUE.toString()}];
    await sc_call(contract, "Set", args);
    const state = await contract.getState();
    expect(state.value).to.be.eq("12");
  });

  it("Should contain event data if Get function is called", async function () {
    const tx = await sc_call(contract, "Emit");
    const receipt = tx.getReceipt()!;
    expect(receipt.event_logs?.length).to.be.eq(1);
    expect(receipt.event_logs![0]._eventname).to.be.eq("Emit");
    expect(receipt.event_logs![0].params).to.deep.contain(
      {type: "Uint128", value: "12", vname: "value"}
    );
  });
});
