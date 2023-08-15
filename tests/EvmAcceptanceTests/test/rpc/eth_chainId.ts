import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";
import hre from "hardhat";

const METHOD = "eth_chainId";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return the chain Id @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedChainId = hre.getEthChainId();
      assert.equal(+result.result, expectedChainId, "should have a chain Id " + expectedChainId);
    });
  });
});
