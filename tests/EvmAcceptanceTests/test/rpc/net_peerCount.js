const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'net_peerCount';

describe("Calling " + METHOD, function () {
  const helper = new ZilliqaHelper()

  it("should return the number of peers connected to the network", async function () {

    await helper.callEthMethod(METHOD, 1, [],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedNetPeerCount = "0x0"
        assert.equal(result.result, expectedNetPeerCount, 'has result:' + result.result + ', expected net_peerCount:' + expectedNetPeerCount);
      })
  })
})