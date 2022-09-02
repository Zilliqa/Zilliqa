var assert = require('chai').assert,
    Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_getBlockTransactionCountByHash';

// TEST
var asyncTest = async function(params, expectedResult){
    await Utils.callEthMethod(method, 1, params, function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');

        if(!result.result) {
            assert.isNull(result.result);
        } else {
            assert.isString(result.result, 'is string');
            assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
            assert.isNumber(+result.result, 'can be converted to a number');

            assert.equal(+result.result, expectedResult, 'should be '+ expectedResult);
        }
    });
};


var asyncErrorTest = async function(){
    await Utils.callEthMethod(method, 1, [], function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'error');
        assert.equal(result.error.code, -32602);
    });
};


describe(method, function () {
    // TODO: Needs fix
    // _.each(config.testBlocks.blocks, function (block) {
    //     it('should return ' + block.transactions.length + ' as a hexstring', function () {
    //         asyncTest(['0x' + block.blockHeader.hash], block.transactions.length);
    //     });
    // });

    it('should return null if the block does not exist', async function () {
        await asyncTest(['0x878a132155f53adb7c993ded4cfb687977397d63d873fcdbeb06c18cac907a5c'], null);
    });

    it('should return an error when no parameter is passed', async function () {
        await asyncErrorTest();
    });
});