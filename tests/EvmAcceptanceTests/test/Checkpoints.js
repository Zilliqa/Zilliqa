const {expect} = require("chai");
const {ethers} = require("hardhat");
const general_helper = require("../helper/GeneralHelper");

describe("Checkpoints functionality", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("CheckpointsMock");
    this.checkpoint = await Contract.deploy();
  });

  it("out of gas test", async function () {
    console.log("#1 step");
    this.tx1 = await this.checkpoint.push(1);
    console.log("#2 step");
    this.tx2 = await this.checkpoint.push(2);
    console.log("#3 step");
    await general_helper.advanceBlock();
    console.log("#4 step");
    this.tx3 = await this.checkpoint.push(3);
    console.log("#5 step");
    await general_helper.advanceBlock();
    console.log("#6 step");
    await general_helper.advanceBlock();
  });
});
