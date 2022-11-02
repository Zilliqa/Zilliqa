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

    it("should return an error on eth_coinbase", async function () {
      await helper.callEthMethod(METHOD, 1, [], (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.isNumber(result.error.code, -32601);
        assert.isString(result.error.message, "is string");
        assert.equal(
          result.error.message,
          "METHOD_NOT_FOUND: The method being requested is not available on this server"
        );
      });
    });
  });
});
