import {assert, expect} from "chai";
import parallelizer from "../helper/Parallelizer";
import {ethers} from "hardhat";
import hre from "hardhat";
import {Contract} from "ethers";
import sendJsonRpcRequest from "../helper/JsonRpcHelper";
import logDebug from "../helper/DebugHelper";

describe("Chained Contract Calls Functionality", function () {
  //const INITIAL_FUND = 10_000_000;
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;

  before(async function () {
    contractOne   = await parallelizer.deployContract("ContractOne");
    contractTwo   = await parallelizer.deployContract("ContractTwo");
    contractThree = await parallelizer.deployContract("ContractThree");
  });

  describe("Install and call chained contracts", function () {

    it("Should create a transaction trace after child creation", async function () {
      const METHOD = "debug_traceTransaction";

      console.log("contractAddr0: ", contractOne.address);
      console.log("contractAddr1: ", contractTwo.address);
      console.log("contractAddr2: ", contractThree.address);

      let res = await contratOne.chainedCall([contractOne.address, contractTwo.address, contractThree.address], 0);

      // Now call contract one, passing in the addresses of contracts two and three

      await sendJsonRpcRequest(METHOD, 1, ["0x00ff"], (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.isString(result.result, "Expected to be populated");
      });
    });

  });
});
