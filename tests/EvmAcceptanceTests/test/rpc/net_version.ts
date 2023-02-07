import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "net_version";

describe("Calling " + METHOD, function () {
  it("should return the current network version", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedNetVersion = hre.getEthChainId();
      assert.equal(
        result.result,
        expectedNetVersion,
        "has result:" + result.result + ", expected net_version:" + expectedNetVersion
      );
    });
  });
});
