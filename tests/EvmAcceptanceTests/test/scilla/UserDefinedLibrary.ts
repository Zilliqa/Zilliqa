const {validation} = require("@zilliqa-js/util");
const {assert, expect} = require("chai");
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

let additionLibAddress: string | undefined;
let mutualLibAddress: string | undefined;
let contract1: ScillaContract;
let contract2: ScillaContract;

describe("Scilla library deploy", () => {
  it("Deploy library - AdditionLib", async () => {
    const libraryName = "AdditionLib";
    const library = await parallelizer.deployScillaLibrary(libraryName);
    additionLibAddress = library.address;
    assert.isTrue(additionLibAddress !== undefined);
    expect(library.address).to.be.properAddress;
    expect(validation.isAddress(additionLibAddress)).to.be.true;
  });

  it("Deploy library - MutualLib.scillib", async () => {
    const libraryName = "MutualLib";
    const library = await parallelizer.deployScillaLibrary(libraryName);
    mutualLibAddress = library.address;
    expect(mutualLibAddress).to.be.properAddress;
    expect(validation.isAddress(mutualLibAddress)).to.be.true;
  });
});

describe("Scilla contract deploy", () => {
  it("Deploy TestContract1 - Import AdditonLib MutualLib", async () => {
    contract1 = await parallelizer.deployScillaContractWithLibrary("TestContract1", [
      {name: "AdditionLib", address: additionLibAddress!},
      {name: "MutualLib", address: mutualLibAddress!}
    ]);

    expect(contract1.address).to.be.properAddress;
    expect(validation.isAddress(contract1.address)).to.be.true;
  });

  it("Deploy TestContract2 - Import MutualLib", async () => {
    contract2 = await parallelizer.deployScillaContractWithLibrary("TestContract2", [
      {name: "MutualLib", address: mutualLibAddress!}
    ]);

    expect(contract2.address).to.be.properAddress;
    expect(validation.isAddress(contract2.address)).to.be.true;
  });
});

describe("Scilla contract execute", () => {
  it("Call TestContract1 - Sending transition", async () => {
    const tx = await contract1.Sending(contract2.address);
    expect(tx.receipt.success).equal(true);
    expect(tx).to.have.eventLog("Bool const of T2 type");
  });
});

// TODO: Enable this whenever https://github.com/Zilliqa/hardhat-scilla-plugin/issues/22 is fixed
describe.skip("Codehash contract", () => {
  let contract: ScillaContract;
  it("Deploy codehash contract", async () => {
    contract = await parallelizer.deployScillaContract("codehash.scilla");
    assert.isTrue(contract.address !== undefined);
  });

  it("Call code hash contract - Foo transition", async () => {
    let tx1 = await contract.foo2(contract.toLowerCase());
    const codeHash1 = tx1.receipt.event_logs[0].params[0].value;
    expect(tx1.receipt.success).equal(true);
    let tx2 = await contract.foo2(contract.toLowerCase());
    const codeHash2 = tx2.receipt.event_logs[0].params[0].value;
    expect(codeHash1 === codeHash2);
  });
});
