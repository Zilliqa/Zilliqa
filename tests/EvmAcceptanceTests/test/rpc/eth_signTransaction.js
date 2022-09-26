const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_signTransaction';

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", async function () {
    before(async function () {
      if (!helper.isZilliqaNetworkSelected()) {
        this.skip();
      }
    })

    it("should return an error on sending a sign transaction request", async function () {
      await helper.callEthMethod(METHOD, 2, ["0xF0C05464f12cB2a011d21dE851108a090C95c755", "0xdeadbeaf"],
        (result, status) => {
          console.log(result);

          assert.equal(status, 200, 'has status code');
          assert.isNumber(result.error.code, -32601);
          assert.isString(result.error.message, 'is string');
          assert.equal(result.error.message, 'METHOD_NOT_FOUND: The method being requested is not available on this server');
        })
    })
  });
})