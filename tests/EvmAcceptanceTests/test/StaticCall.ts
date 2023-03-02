import {assert, expect} from "chai";
import {Contract} from "ethers";
import {parallelizer} from "../helpers";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";

describe("Static Contract Calls Functionality", function () {
  let called: Contract;
  let caller: Contract;

  before(async function () {
    called = await parallelizer.deployContract("Called");
    caller = await parallelizer.deployContract("Caller");
  });

  describe("Static calls to contracts should not modify their value", function () {

    it("The value of the called contract should not change", async function () {
      let calledAddress = called.address.toLowerCase();

      // Initial number is contructed as 0
      let contractNum = await called.getNumber();
      assert.equal(contractNum, 0);

      // Static call to contract should not increase it/work
      let res = await caller.callCalled(calledAddress);
      contractNum = await called.getNumber();
      assert.equal(contractNum, 0);
      });

  });
});
