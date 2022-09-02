var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_gasPrice';

// TEST
var asyncTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');
    });
};

describe(method, function () {
    it('should return a number as hexstring', async function () {
        await asyncTest();
    });
});