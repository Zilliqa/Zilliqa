const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_sign';

describe("Calling " + METHOD, function () {
  it("should return a signature", async function () {

    await helper.callEthMethod(METHOD, 2, ["0xF0C05464f12cB2a011d21dE851108a090C95c755", "0xdeadbeaf"],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');

        const expectedSignature = "0xe7f3c5b9d8cc2136a53750cafecbadbc76f5d20f3f873c4756cd8b3b0f115eac0220692a5f86ff9a43c064466128afb6175ff4f73660aa85644fa54e53dd3f0d1c";
        assert.equal(+result.result, expectedSignature, 'should have a signature ' + expectedSignature);
      })
  })
})