var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_getTransactionCount';

// TEST
var asyncTest = async function(params, expectedResult){
    await Utils.callEthMethod(method, 1, params, function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        assert.equal(+result.result, expectedResult, 'should be '+ expectedResult);
    });
};


var asyncErrorTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'error');
        assert.equal(result.error.code, -32602);
    });
};



describe(method, function(){
    // TODO: Needs fix
    // it('should return the current number of transactions as a hexstring when the defaultBlock is 0', async function () {
    //     asyncTest(['0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b', '0x0'], +config.testBlocks.pre['a94f5374fce5edbc8e2a8697c15331677e6ebf0b'].nonce);
    // });

    // it('should return the current number of transactions as a hexstring when the defaultBlock is "latest"', async function () {
    //     asyncTest(['0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b', 'latest'], +config.testBlocks.postState['a94f5374fce5edbc8e2a8697c15331677e6ebf0b'].nonce);
    // });

    it('should return an error when no parameter is passed', function () {
        asyncErrorTest();
    });
});