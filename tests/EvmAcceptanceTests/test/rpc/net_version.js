const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "net_version";

describe("Calling " + METHOD, function () {
  it("should return the current network version", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      hre.logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedNetVersion = helper.getEthChainId();
      assert.equal(
        result.result,
        expectedNetVersion,
        "has result:" + result.result + ", expected net_version:" + expectedNetVersion
      );
    });
  });
});
