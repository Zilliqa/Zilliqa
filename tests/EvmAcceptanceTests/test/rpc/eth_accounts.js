var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_accounts';

// TEST
var asyncTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isArray(result.result, 'is array');
        // we should have at least one account here (coinbase)
        assert.isTrue(Utils.isAddress(result.result[0]));
    });
};

describe(method, function(){
    it('should return an array with accounts', async function () {
        await asyncTest();
    });
});