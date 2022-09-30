const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "eth_mining";

describe("Calling " + METHOD, function () {
  it("should return the mining state", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      if (hre.debugMode) {
        console.log(result);
      }

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isBoolean(result.result, "is boolean");

      const expectedMiningState = helper.getMiningState();
      assert.equal(+result.result, expectedMiningState, "should have a chain Id " + expectedMiningState);
    });
  });
});
