const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'eth_protocolVersion';

describe("Calling " + METHOD, function () {
  const helper = new ZilliqaHelper()

  it("should return the protocol version", async function () {


    await helper.callEthMethod(METHOD, 1, [],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedProtocolVersion = helper.getProtocolVersion();
        console.log(expectedProtocolVersion);
        assert.equal(+result.result, expectedProtocolVersion, 'has result:' + result.result + ', expected eth_protocolVersion:' + expectedProtocolVersion);

      })
  })
})