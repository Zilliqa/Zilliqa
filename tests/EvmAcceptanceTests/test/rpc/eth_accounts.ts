import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_accounts";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return an array with accounts @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      // block number changes at every call, so check only the response format
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isArray(result.result, "is array");
    });
  });
});
