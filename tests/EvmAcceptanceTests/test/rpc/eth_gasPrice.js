const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_gasPrice';

describe("Calling " + METHOD, function () {
 

  it("should return the gasPrice as specified in the ethereum protocol", async function () {
    const expectedGasPrice = 2000000000 // default ganache gas price in wei

    await helper.callEthMethod(METHOD, 1, [],
      (result, status) => {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        assert.equal(+result.result, expectedGasPrice, 'should have a gas price ' + expectedGasPrice);
      })
  })
})