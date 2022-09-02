var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_getCompilers';

// TEST
var asyncTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isArray(result.result, 'is array');
        // assert.include(result.result, "lll");
        // assert.include(result.result, "solidity");
        // assert.include(result.result, "serpent");
    });
};

describe(method, function(){
    it('should return an array with compilers', async function () {
        await asyncTest();
    });
});