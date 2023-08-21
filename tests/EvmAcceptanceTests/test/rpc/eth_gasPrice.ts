import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_gasPrice";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return the gasPrice as specified in the ethereum protocol @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is not a string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedMinGasPrice = 1000000; // default minimum Zilliqa gas price in wei
      assert.isAtLeast(+result.result, expectedMinGasPrice, "should have a gas price " + expectedMinGasPrice);
    });
  });
});
