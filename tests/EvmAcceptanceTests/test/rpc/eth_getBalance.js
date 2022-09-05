const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;

const METHOD = 'eth_getBalance';

describe("Calling " + METHOD, function () {
    const helper = new ZilliqaHelper()

    it("should return the balance as specified in the ethereum protocol", async function () {
        const expectedBalance = 1 * (10 ** 18) // should be 1 eth in wei

        await helper.callEthMethod(METHOD, 1, [
            "0xF0C05464f12cB2a011d21dE851108a090C95c755", // public address
            "latest"],
            (result, status) => {
                assert.equal(status, 200, 'has status code');
                assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                assert.isString(result.result, 'is string');
                assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                assert.isNumber(+result.result, 'can be converted to a number');

                assert.equal(+result.result, expectedBalance, 'should have balance ' + expectedBalance);
            })

        await helper.callEthMethod(METHOD, 1, [
            "0xF0C05464f12cB2a011d21dE851108a090C95c755", // public address
            "earliest"],
            (result, status) => {
                assert.equal(status, 200, 'has status code');
                assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                assert.isString(result.result, 'is string');
                assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                assert.isNumber(+result.result, 'can be converted to a number');

                assert.equal(+result.result, expectedBalance, 'should have balance ' + expectedBalance);
            })

        await helper.callEthMethod(METHOD, 1, [
            "0xF0C05464f12cB2a011d21dE851108a090C95c755", // public address
            "pending"],
            (result, status) => {
                assert.equal(status, 200, 'has status code');
                assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                assert.isString(result.result, 'is string');
                assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                assert.isNumber(+result.result, 'can be converted to a number');

                assert.equal(+result.result, expectedBalance, 'should have balance ' + expectedBalance);
            })


        await helper.callEthMethod(METHOD, 1, [
            "0xF0C05464f12cB2a011d21dE851108a090C95c755",   // public address
            "unknown tag"],                                 // not supported tag should give an error
            (result, status) => {
                assert.equal(status, 200, 'has status code');
                // todo check the error result to expect              assert.property(result.error, 'error');
            })
    })
})