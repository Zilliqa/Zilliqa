import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

function printEvents(receipt) {
  receipt.events.forEach( (el) => {
    if (el.event !== undefined) { 
      console.log(`${el.event} ${JSON.stringify(el.args)}`);
    }
  });
}


describe("DelegateDouble", async function () {
  before(async function () {
    this.contractA = await parallelizer.deployContract("DDContractA");
    this.contractBProx = await parallelizer.deployContract("DDContractB");
    this.contractC = await parallelizer.deployContract("DDContractC");
    this.contractDProx = await parallelizer.deployContract("DDContractD");
    this.contractB = await hre.ethers.getContractAt("DDContractA", this.contractBProx.address);
    this.contractD = await hre.ethers.getContractAt("DDContractC", this.contractDProx.address);
    console.log(`${this.contractA.address} ${this.contractB.address} ${this.contractC.address} ${this.contractD.address}`);
    // Now set up the implementations.
    await this.contractBProx.setImplementation(this.contractA.address);
    await this.contractDProx.setImplementation(this.contractC.address, this.contractB.address);
    this.owningAddress = ethers.BigNumber.from(this.contractD.address);
    console.log(`Num = ${this.owningAddress.toHexString()}`);
  });

  it("Should be possible to set the owner", async function() {

    let tx = await this.contractD.setOwner(this.owningAddress);
    let receipt = await tx.wait();
    printEvents(receipt);
    console.log(`${JSON.stringify(receipt)}`);
  });

  it("Should be possible to check the owner", async function() {
    let tx = await this.contractD.owner();
    let receipt = await tx.wait();
    printEvents(receipt);
    // console.log(`${JSON.stringify(receipt.events)}`);
  });

  it("Should have state stored in the right place", async function() {
    let tx = await this.contractA.getOwnerX(this.owningAddress);
    let receipt_a = await tx.wait();
    // Should be 0.
    printEvents(receipt_a);

    let tx_b = await this.contractBProx.getOwnerX(this.owningAddress);
    let receipt_b = await tx_b.wait();
    // Should have state
    printEvents(receipt_b);
  });
});

