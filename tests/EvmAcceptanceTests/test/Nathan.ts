import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers, upgrades} from "hardhat";

describe("Delegatecall functionality", function () {
  before(async function () {
    //// Final level
    //this.testDelegateContract = await parallelizer.deployContract("TestDelegatecall");

    //// Second level
    //this.delegateContract = await parallelizer.deployContract("Delegatecall");

    //// First level
    //this.baseDelegator = await parallelizer.deployContract("BaseDelegator");
  });

  it("xxx", async function () {
    const SLICES = 1000000;
    //
    //this.nathan = await parallelizer.deployContract("Nathan");

   const Nathan = await ethers.getContractFactory("Nathan");

   console.log("Deploying Nathan...");

   const nathan = await upgrades.deployProxy(Nathan, [SLICES], {
     initializer: "initialize",
   });
   await nathan.deployed();

   console.log("Nathan deployed to:", nathan.address);

  });
});
