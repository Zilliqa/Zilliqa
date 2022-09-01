const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'eth_getBalance';

describe("Calling " + METHOD, function () {
    const helper = new ZilliqaHelper()

    it("should return the balance as specified in the ethereum protocol", async function() {
        const expectedResult = 0
        await helper.callEthMethod(METHOD, 1, [
            "0xF0C05464f12cB2a011d21dE851108a090C95c755", "latest"
        ],(result, status) => {
            assert.equal(status, 200, 'has status code');
            assert.property(result, 'result', (result.error) ? result.error.message : 'error');
            assert.isString(result.result, 'is string');
            assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
            assert.isNumber(+result.result, 'can be converted to a number');

            assert.equal(+result.result, expectedResult, 'should have balance '+ expectedResult);
        })
    })

})