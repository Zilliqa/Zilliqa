var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_coinbase';

// TEST
var asyncTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isTrue(result.result === null || Utils.isAddress(result.result), 'is coinbase address');

        config.coinbase = result.result;
    });
};


describe(method, async function () {
    it('should return a coinbase address', async function () {
        await asyncTest();
    });
});