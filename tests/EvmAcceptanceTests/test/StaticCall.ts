import {assert, expect} from "chai";
import {Contract} from "ethers";
import {parallelizer} from "../helpers";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";

describe("Static Contract Calls Functionality", function () {
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;

  before(async function () {
    called = await parallelizer.deployContract("Called");
    caller = await parallelizer.deployContract("Caller");
  });

  describe("xxx", function () {

    it("yyy", async function () {
      let calledAddress = called.address.toLowerCase();

      let res = await caller.callCalled(calledAddress);

      console.log(res);

      });

  });
});
