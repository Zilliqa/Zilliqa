const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "net_listening";

describe("Calling " + METHOD, function () {
  it("should return the network listening state", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      if (hre.debugMode) {
        console.log(result);
      }

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isBoolean(result.result, "can be converted to a boolean");

      const expectedNetListing = true;
      assert.equal(
        result.result,
        expectedNetListing,
        "has result:" + result.result + ", expected net_listening:" + expectedNetListing
      );
    });
  });
});
