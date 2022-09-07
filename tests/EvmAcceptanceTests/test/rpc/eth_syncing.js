const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_syncing';

describe("Calling " + METHOD, function () {
  it("should return the syncing state", async function () {
    await helper.callEthMethod(METHOD, 1, [],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isBoolean(result.result, 'is boolean');

        const expectedSyncingState = false;
        assert.equal(+result.result, expectedSyncingState, 'should have a syncing state ' + expectedSyncingState);
      })
  })
})