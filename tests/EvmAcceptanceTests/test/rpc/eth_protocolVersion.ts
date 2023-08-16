import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_protocolVersion";

describe("Calling " + METHOD, function () {
  it("should return the protocol version", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedProtocolVersion = hre.getProtocolVersion();
      assert.equal(
        +result.result,
        expectedProtocolVersion,
        "has result:" + result.result + ", expected eth_protocolVersion:" + expectedProtocolVersion
      );
    });
  });
});
