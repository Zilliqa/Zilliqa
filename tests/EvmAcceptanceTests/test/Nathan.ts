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
    const OWNER = 1;
    //
    //this.nathan = await parallelizer.deployContract("Nathan");

   console.log("Getting contract A...");

   const contractA = await ethers.getContractFactory("A");

   console.log("Deploying contract A...");

   const contA = await upgrades.deployProxy(contractA, [OWNER], {
     initializer: "initialize",
   });
   await contA.deployed();

   console.log("Contract A proxy deployed to:", contA.address);

   // This should succeed
   let zz = await contA.ownerOf(OWNER);
   console.log(zz);

   const contractC = await ethers.getContractFactory("C");

   console.log("Deploying contract C...");

   const contC = await upgrades.deployProxy(contractC, [contA.address], {
     initializer: "initialize",
   });
   await contC.deployed();

   console.log("Contract C proxy deployed to:", contC.address);

   zz = await contC.ownerTest();
   console.log(zz);

  });
});
