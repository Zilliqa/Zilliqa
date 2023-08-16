import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_mining";

describe("Calling " + METHOD, function () {
  it("should return the mining state", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isBoolean(result.result, "is boolean");

      const expectedMiningState = hre.getMiningState();
      assert.equal(Boolean(result.result), expectedMiningState, "should have a chain Id " + expectedMiningState);
    });
  });
});
