const {assert, expect} = require("chai");
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("Codehash contract", () => {
  let contract: ScillaContract;
  it("Deploy codehash contract", async () => {
    contract = await parallelizer.deployScillaContract("Codehash");
    assert.isTrue(contract.address !== undefined);
  });

  it("Call code hash contract - Foo transition", async () => {
    let tx1 = await contract.foo(contract.address!.toLowerCase());
    const codeHash1 = tx1.receipt.event_logs[0].params[0].value;
    expect(tx1.receipt.success).equal(true);
    let tx2 = await contract.foo2(contract.address!.toLowerCase());
    const codeHash2 = tx2.receipt.event_logs[0].params[0].value;
    expect(codeHash1).to.be.eq(codeHash2);
  });
});