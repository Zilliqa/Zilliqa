import {expect} from "chai";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";
import hre from "hardhat";

describe("Scilla contract deployment with error", () => {
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }
  });

  it("should return error if the deployed contract has a syntax error", async () => {
    let contract: ScillaContract = await parallelizer.deployScillaContract("AdditionLibErrored");
    let tx = contract.deployed_by;
    expect(contract.address).to.be.undefined;
    expect(tx.receipt.exceptions[0].message).to.include("match-expression is probably missing `end` keyword.");
  });

  // Disabled in q4-working-branch
  xit("should return error if the provided init version is invalid", async () => {
    let contractPath = hre.scillaContracts["SetGet"].path;
    const init = [{vname: "_scilla_version", type: "Uint32", value: "1"}];

    let [tx, contract] = await hre.deployScillaFile(contractPath, init);
    expect(contract.address).to.be.undefined;
    expect(tx.getReceipt()).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions![0].message).to.include("Scilla version mismatch");
  });

  it("should return error if init doesn't contain parameter needed for contract deployment", async () => {
    let contractPath = hre.scillaContracts["ImmutableString"].path;
    const init = [{vname: "_scilla_version", type: "Uint32", value: "0"}];

    let [tx, contract] = await hre.deployScillaFile(contractPath, init);
    expect(contract.address).to.be.undefined;
    expect(tx.getReceipt()).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions![0].message).to.include("No init entry found matching contract");
  });

  it("should return error if the provided parameter to contract deployment doesn't match", async () => {
    let contractPath = hre.scillaContracts["ImmutableString"].path;
    const init = [
      {vname: "_scilla_version", type: "Uint32", value: "0"},
      {vname: "immutable_string", type: "Uint32", value: "3"}
    ];

    let [tx, contract] = await hre.deployScillaFile(contractPath, init);
    expect(contract.address).to.be.undefined;
    expect(tx.getReceipt()).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions).not.to.be.undefined;
    expect(tx.getReceipt()!.exceptions![0].message).to.include(
      "Type unassignable: String expected, but Uint32 provided"
    );
  });
});
