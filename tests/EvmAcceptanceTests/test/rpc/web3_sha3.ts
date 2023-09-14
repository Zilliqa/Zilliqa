import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "web3_sha3";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return a sha3 of the provided hex string @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, ["0x68656c6c6f20776f726c64"], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      const expectedSha3 = "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad";
      assert.equal(result.result, expectedSha3, "has result:" + result.result + ", expected sha3:" + expectedSha3);
    });
  });
});
