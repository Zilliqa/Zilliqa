var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_protocolVersion';

// TEST
var asyncTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
    });
};

describe(method, function () {
    it('should return a version number as HEX string', async function () {
        await asyncTest();
    });
});

