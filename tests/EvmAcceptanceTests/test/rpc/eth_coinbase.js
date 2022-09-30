const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "eth_coinbase";

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", function () {
    before(async function () {
      if (!helper.isZilliqaNetworkSelected()) {
        this.skip();
      }
    });

    it("should return the coin base", async function () {
      await helper.callEthMethod(METHOD, 1, [], (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");

        const expectedCoinBase = "0x0000000000000000000000000000000000000000";
        assert.equal(+result.result, expectedCoinBase, "should have a coin base " + expectedCoinBase);
      });
    });
  });
});
