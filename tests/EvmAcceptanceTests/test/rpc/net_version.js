const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'net_version';

describe("Calling " + METHOD, function () {
  const helper = new ZilliqaHelper()

  it("should return the current network version", async function () {

    await helper.callEthMethod(METHOD, 1, [],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedNetVersion = "3" // zilliqa network id
        assert.equal(result.result, expectedNetVersion, 'has result:' + result.result + ', expected net_version:' + expectedNetVersion);
      })
  })
})