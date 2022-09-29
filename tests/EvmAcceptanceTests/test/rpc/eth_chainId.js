const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "eth_chainId";

describe("Calling " + METHOD, function () {
  it("should return the chain Id", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedChainId = helper.getEthChainId();
      assert.equal(+result.result, expectedChainId, "should have a chain Id " + expectedChainId);
    });
  });
});
