import {assert} from "chai";
import {Contract} from "ethers";
import hre from "hardhat";

describe("Static Contract Calls Functionality #parallel", function () {
  let called: Contract;
  let caller: Contract;

  before(async function () {
    called = await hre.deployContract("Called");
    caller = await hre.deployContract("Caller");
  });

  describe("Static calls to contracts should not modify their value", function () {
    it("The value of the called contract should not change @block-1", async function () {
      let calledAddress = called.address.toLowerCase();

      // Initial number is constructed as 0
      let contractNum = await called.getNumber();
      assert.equal(contractNum, 0);

      // Static call to contract should not increase it/work
      let res = await caller.callCalled(calledAddress);
      contractNum = await called.getNumber();
      assert.equal(contractNum, 0);
    });
  });

  describe("Static calls to contracts should not modify their value when chained", function () {
    it("The value of the called contract should not change @block-1", async function () {
      let callerAddress = caller.address.toLowerCase();
      let calledAddress = called.address.toLowerCase();

      // Initial number of 'called' contract is constructed as 0
      let contractNum = await called.getNumber();
      assert.equal(contractNum, 0);

      // This contract calls 'Caller' which makes a static call back to itself. This should
      // not change its value (but will if static is not respected)
      let res = await called.callCaller(callerAddress, calledAddress);

      contractNum = await called.getNumber();
      assert.equal(contractNum, 0);
    });
  });
});
