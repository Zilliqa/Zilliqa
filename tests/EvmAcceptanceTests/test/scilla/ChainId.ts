const {expect} = require("chai");
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";
import hre from "hardhat";

describe("ChainId contract", () => {
  let contract: ScillaContract;

  it("Deploy chainId contract", async () => {
    contract = await parallelizer.deployScillaContract("ChainId");
    expect(contract.address).to.be.properAddress;
  });

  it("Call chain id contract -  EventChainId", async () => {
    const tx = await contract.EventChainID();

    expect(tx).to.have.eventLogWithParams("ChainID", {value: hre.getZilliqaChainId()});
  });
});
