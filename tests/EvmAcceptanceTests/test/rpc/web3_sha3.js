const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'web3_sha3';

describe("Calling " + METHOD, function () {
  const helper = new ZilliqaHelper()

  it("should return a sha3 of the provided hex string", async function () {

    //const expectedSha3 = "0x47173285a8d7341e5e972fc677286384f802f8ef42a5ec5f03bbfa254cb01fad" 
    const expectedSha3 = '0xb1e9ddd229f9a21ef978f6fcd178e74e37a4fa3d87f453bc34e772ec91328181'

    await helper.callEthMethod(METHOD, 1, ["0x68656c6c6f20776f726c64"],
      (result, status) => {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        assert.equal(+result.result, expectedSha3, 'should have a sha3 ' + expectedSha3);
      })
  })
})