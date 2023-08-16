import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_syncing";

describe("Calling " + METHOD, function () {
  it("should return the syncing state", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isBoolean(result.result, "is boolean");

      const expectedSyncingState = false;
      assert.equal(Boolean(result.result), expectedSyncingState, "should have a syncing state " + expectedSyncingState);
    });
  });
});
