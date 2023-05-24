import {expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";

const DEBUG = false;

interface EventRequirement {
  from : AbstractContract;
  eventName : String;
  eventArgs: any[]
};

function arraysEqual(arr0 : any[], arr1: any[]) {
  if (arr0 === undefined && arr1 === undefined) {
    return true;
  }
  if (arr0 === undefined || arr1 === undefined) {
    return false;
  }
  if (arr0.length != arr1.length) {
    return false;
  }
  for (let i=0;i<arr0.length; ++i) {
    if (arr0[i] != arr1[i]) {
      return false;
    }
  }
  return true;
}



function hasEvent(receipt, require: EventRequirement) { 
  let found = false;
  receipt.events.forEach( (el) => {
    if (!found) {
      if (DEBUG) {
        console.log(`Checking: ${JSON.stringify(el)} vs ${require.eventName} / ${require.from.address} / ${JSON.stringify(require.eventArgs)}`);
        console.log(`event ${el.event} l ${el.args.length} ev ${JSON.stringify(el.args)} addr ${el.address.toString()}`);
        console.log(`eq ${arraysEqual(require.eventArgs,el.args)}`)
        console.log(`***********`);
      }
      if (el.event === require.eventName 
        && arraysEqual(el.args,require.eventArgs)
        && el.address.toString().toLowerCase() === require.from.address.toLowerCase()) {
        if (DEBUG) {
          console.log(`Found!`);
        }
        found = true;
      }
    }
  });
  return found;
}

function makeEvent(contract : AbstractContract, name : String, ...args : any[]) {
  let result : EventRequirement = {
    from: contract,
    eventName: name,
    eventArgs: args
  }
  return result;
}

function printEvents(receipt) {
  receipt.events.forEach( (el) => {
    if (el.event !== undefined) { 
      console.log(`${el.event} ${el.address} ${el.args} ${JSON.stringify(el.args)}`);
    }
  });
}


describe("DelegateDouble", async function () {
  this.timeout(800_000);
  before(async function () {
    const [ sig ] = await ethers.getSigners();
    this.signer = sig;
    this.contractA = await parallelizer.deployContractWithSigner(this.signer, "DDContractA");
    this.contractBProx = await parallelizer.deployContractWithSigner(this.signer, "DDContractB");
    this.contractC = await parallelizer.deployContractWithSigner(this.signer, "DDContractC");
    this.contractDProx = await parallelizer.deployContractWithSigner(this.signer, "DDContractD");
    this.contractB = await hre.ethers.getContractAt("DDContractA", this.contractBProx.address);
    this.contractD = await hre.ethers.getContractAt("DDContractC", this.contractDProx.address);

    if (DEBUG) {
      console.log(`${this.contractA.address} ${this.contractB.address} ${this.contractC.address} ${this.contractD.address}`);
    }
    // Now set up the implementations.
    await this.contractBProx.setImplementation(this.contractA.address);
    await this.contractDProx.setImplementation(this.contractC.address, this.contractB.address);
    this.owningAddress = ethers.BigNumber.from(this.contractD.address);
    if (DEBUG) {
      console.log(`Owner = ${this.owningAddress.toHexString()}`);
    }
  });

  it("Should be possible to set the owner", async function() {
    let tx = this.contractD.setOwner(this.owningAddress);
    let result = await tx;
    let receipt = await result.wait();

    // I tried to do this with expect(x).to.emit(y), but utterly failed to
    // make it work - test passed no matter what I did - rrw 2023-05-22.

    // Contract C (proxied by D) gets called.
    expect(hasEvent(receipt, makeEvent(this.contractD, "Msg", "SetOwnerC"))).to.be.true;
    // Contract A (proxied by B) gets called.
    expect(hasEvent(receipt, makeEvent(this.contractB, "Msg", "SetOwnerA"))).to.be.true;
    // And returns
    expect(hasEvent(receipt, makeEvent(this.contractD, "Msg", "SetOwnerC done"))).to.be.true;
  });

  it("Should be possible to check the owner", async function() {
    let tx = await this.contractD.owner();
    let receipt = await tx.wait();

    expect(hasEvent(receipt, makeEvent(this.contractD, "Msg", "Hello"))).to.be.true;
    expect(hasEvent(receipt, makeEvent(this.contractD, "Value", this.contractD.address.toString()))).to.be.true;
  });

  it("Should have state stored in the right place", async function() {
    let tx = await this.contractA.getOwnerX(this.owningAddress);
    let receipt_a = await tx.wait();
    // Should be 0.
    expect(hasEvent(receipt_a, makeEvent(this.contractA, "Msg", "AOwnerX"))).to.be.true;
    expect(hasEvent(receipt_a, makeEvent(this.contractA, "Value", "0x0000000000000000000000000000000000000000"))).to.be.true;

    let tx_b = await this.contractBProx.getOwnerX(this.owningAddress);
    let receipt_b = await tx_b.wait();
    // check that the storage is in the proxy.
    expect(hasEvent(receipt_b, makeEvent(this.contractB, "Msg", "BOwnerX"))).to.be.true;
    expect(hasEvent(receipt_b, makeEvent(this.contractB, "Value", this.contractD.address.toString()))).to.be.true;
  });
});

