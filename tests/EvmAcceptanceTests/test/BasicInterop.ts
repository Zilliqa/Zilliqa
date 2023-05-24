import {expect} from "chai";
import {Contract} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";

const DEBUG = false;

/** DANGER WILL ROBINSON!
 * There are a lot of catch (e: Exception) { } here. Many of these exist because current code aborts a transaction
 * rather than reverting it. When that bug is fixed as part of EVM interop, we should instead assert that our
 * contract calls don't throw - rrw 2023-05-18
 */
describe("BasicInterop", function () {
  // Keys used in all tests cases
  const addr1 = "0xB3F90B06a7Dd9a860f8722f99B17fAce5abcb259";
  const addr2 = "0xc8532d4c6354D717163fAa8B7504b2b4436D20d1";

  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContract2: ScillaContract;
  let scillaContractAddress: string;
  let scillaContract2Address: string;

  before(async function () {
    solidityContract = await parallelizer.deployContract("BasicInterop");

    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    scillaContract = await parallelizer.deployScillaContract("BasicInterop");
    scillaContractAddress = scillaContract.address?.toLowerCase()!;
    scillaContract2 = await parallelizer.deployScillaContract("BasicInterop");
    scillaContract2Address = scillaContract2.address?.toLowerCase()!;
  });

  it("Should be deployed successfully", async function () {
    expect(solidityContract.address).to.be.properAddress;
    expect(scillaContract.address).to.be.properAddress;
    expect(scillaContract2.address).to.be.properAddress;
  });

  describe("Interop message semantics", async function () {
    beforeEach(async function() {
      if (DEBUG) {
        console.log("Resetting counters");
      }
      await solidityContract.callUint(scillaContractAddress, "setUint", 0);
      await solidityContract.callUint(scillaContract2Address, "setUint", 0);
    });

    it("Always reverts if the called contract throws", async function () {
      try {
        let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "fail");
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      if (DEBUG) {
        console.log(`NUM ${readRes} NUM2 ${readRes2}`);
      }
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });

    it("Reverts if you call a contract which sends two messages, one of which is handled and throws", async function() {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect2", "ThisIsNotHandled")
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      console.log(`NUM ${readRes} NUM2 ${readRes2}`);
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });

    // This is a horrid semantic! It means that if any of your messages are handled, they must all be, or we will fail out the
    // transaction.
    it("Correctly processes a call where two messages are sent one of which is handled", async function() {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect3", "ThisIsNotHandled")
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });

    it("Correctly processes a call where two messages are sent and handled", async function() {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect2", "doNothing");
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(1);
      expect(readRes2).to.equal(2);
    });

    it("Reverts if the called contract calls a contract which doesn't handle the message", async function () {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect", "ThisIsNotHandled")
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });


    it("Always reverts if you call a contract which sends a message whose handler throws", async function () {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect", "failAndSendMessage");
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      console.log("C");
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });

    it("Propagates messages correctly if you call a contract which sends a message whose handler succeeds", async function () {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect", "doNothing");
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(1);
      expect(readRes2).to.equal(1);
    });

    it("Fails if you call a contract which sends a message whose handler cannot be found", async function () {
      try {
        let result = await solidityContract.callIndirectAndIgnoreResult(scillaContractAddress, scillaContract2Address, "Indirect", "sendAMessage");
        let val = await result.wait();
      } catch (e: Exception) {
        // TODO - see comment at top of describe()
      }
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      let readRes2 = await solidityContract.readUint(scillaContract2Address, "uintField");
      expect(readRes).to.equal(0);
      expect(readRes2).to.equal(0);
    });
  });

  describe("Interop call semantics", function () {
    beforeEach(async function() {
      await solidityContract.callUint(scillaContractAddress, "setUint", 0);
    });

    it("ignores sending a message", async function () {
      let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "sendAMessage");
      let val = await result.wait();
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.equal(1);
    });

    it("Ignores an error", async function () {
      let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "fail");
      let val = await result.wait();
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.equal(0);
    });

    it("Can cope with messages and failure", async function () {
      let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "failAndSendMessage");
      let val = await result.wait();
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.equal(0);
    });

    it("Can cope with failure and events", async function () {
      let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "failAndSendBoth");
      let val = await result.wait();
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.equal(0);
    });

    it("Does nothing when asked", async function() {
      let result = await solidityContract.callAndIgnoreResult(scillaContractAddress, "doNothing");
      let val = await result.wait();
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.equal(1);
    });
  });

  describe("When call is performed from solidity to scilla contract", function () {
    it("It should return proper string after invoking set method with string arg", async function () {
      const someString = "SomeString";
      await solidityContract.callString(scillaContractAddress, "setString", someString);
      let readString = await solidityContract.readString(scillaContractAddress, "strField");
      expect(readString).to.be.equal(someString);
    });

    it("It should return proper integer after invoking set method for simpleMap", async function () {
      const VAL = 1000;
      await solidityContract.callSimpleMap(scillaContractAddress, "setSimpleMap", addr1, VAL);
      let readRes = await solidityContract.readSimpleMap(scillaContractAddress, "simpleMap", addr1);
      expect(readRes).to.be.eq(VAL);
    });

    it("It should return proper integer after invoking set method for nestedMap", async function () {
      const VAL = 2000;
      await solidityContract.callNestedMap(scillaContractAddress, "setNestedMap", addr1, addr2, VAL);
      let readRes = await solidityContract.readNestedMap(scillaContractAddress, "nestedMap", addr1, addr2);
      expect(readRes.toNumber()).to.be.eq(VAL);
    });

    it("It should return proper integer after invoking set method with integer arg", async function () {
      const NUM = 12345;
      await solidityContract.callUint(scillaContractAddress, "setUint", NUM);
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.be.eq(NUM);
    });
  });

  // Non-existing keys

  it("It should return revert while reading non-existing  field", async function () {
    expect(solidityContract.readString(scillaContractAddress, "UknownField")).to.be.reverted;
  });

  it("It should return 0 while reading non-existing key in simpleMap", async function () {
    const VAL = 0;
    let readRes = await solidityContract.readSimpleMap(scillaContractAddress, "simpleMap", addr2);
    expect(readRes.toNumber()).to.be.eq(VAL);
  });

  it("It should return 0 while reading non-existing key in nestedMap", async function () {
    const VAL = 0;
    let readRes = await solidityContract.readNestedMap(scillaContractAddress, "nestedMap", addr2, addr1);
    expect(readRes.toNumber()).to.be.eq(VAL);
  });
});
