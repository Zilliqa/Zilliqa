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

  it("Deploy library - Addition_Errored.scillib", async () => {
    const libraryName = "AdditionLibErrored";
    let library;
    try {
      library = await parallelizer.deployScillaLibrary(libraryName);
    } catch (_) {}

    expect(library).to.be.undefined;
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

// TODO: Enable this whenever https://github.com/Zilliqa/hardhat-scilla-plugin/issues/22 is fixed
describe.skip("Ecdsa contract", () => {
  let contract: ScillaContract;
  it("Deploy ecdsa contract", async () => {
    contract = await parallelizer.deployScillaContract("ecdsa.scilla");
    assert.isTrue(contract.address !== undefined);
  });

  it("Call rcdsa contract -  recover invalid input and failed", async () => {
    const tx = await contract.recover(
      "0x1beedbe103d0b0da3f0ff6f8b614569c92174fb82e04c6676f9aa94b994774c5",
      "0x5f2afac816d9430bce53e081667378790bb5b703ea4a98234649ccac8a358f7a262553d9df46d8417239138bc69db8c458620093b2124937c6a5af2d86f0014e",
      32
    );

    expect(tx.receipt.success).equal(false);
    assert.include(tx.receipt.exceptions[0].message, "Sign.read_recoverable_exn: recid must be 0, 1, 2 or 3");
  });

  it("Call rcdsa contract -  recover valid input and failed", async () => {
    const tx = await contract.recover(
      "0x1beedbe103d0b0da3f0ff6f8b614569c92174fb82e04c6676f9aa94b994774c5",
      "0x5f2afac816d9430bce53e081667378790bb5b703ea4a98234649ccac8a358f7a262553d9df46d8417239138bc69db8c458620093b2124937c6a5af2d86f0014e",
      2
    );

    expect(tx.receipt.success).equal(false);
    assert.include(tx.receipt.exceptions[0].message, "Sign.recover: pk could not be recovered");
  });
});
