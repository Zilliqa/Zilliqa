const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'eth_estimateGas';

describe("Calling " + METHOD, function () {
  const helper = new ZilliqaHelper()

  it("should return the estimated gas as calculated over the transaction provided", async function () {
    const estimatedGas = 2000000000

    await helper.callEthMethod(METHOD, 2, [
      "{\"from\":, \"to\":, \"value\":, \"gas\":, \"data\":}", "latest"
    ], (result, status) => {
      assert.equal(status, 200, 'has status code');
      assert.property(result, 'result', (result.error) ? result.error.message : 'error');
      assert.isString(result.result, 'is string');
      assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
      assert.isNumber(+result.result, 'can be converted to a number');

      assert.equal(+result.result, estimatedGas, 'should have an estimated gas' + estimatedGas);
    })
  })

})