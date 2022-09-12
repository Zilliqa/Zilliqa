const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_sendRawTransaction';

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", async function () {
    // before(async function () {
    //   if (!helper.isZilliqaNetworkSelected()) {
    //     this.skip();
    //   }
    // })

    it("should return a send raw transaction", async function () {
      await helper.callEthMethod(METHOD, 1, ["0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675"],
        (result, status) => {
          console.log(result);

          assert.equal(status, 200, 'has status code');
          //assert.isNumber(result.error.code, -32601);
          assert.isString(result.error.message, 'is string');
          //assert.equal(result.error.message, 'METHOD_NOT_FOUND: The method being requested is not available on this server');
        })
    })
  });
})