var assert = require('chai').assert,
    Utils = require('../../helper/Utils')
    _ = require('underscore');

// METHOD
var method = 'eth_getCode';

// TEST
var asyncTest = async function(params, expectedResult){
    await Utils.callEthMethod(method, 1, params, function(result, status) {
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');

        assert.equal(result.result, expectedResult, 'should be '+ expectedResult);
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
    // FIXME:
    // Helpers.eachHost(function (key, host) {
    //     _.each(config.testBlocks.postState, function (state, key) {
    //         it('should return the code when defaultBlock is "latest" at 0x' + key, function () {
    //             asyncTest(['0x' + key, 'latest'], state.code);
    //         });
    //     });

    //     _.each(config.testBlocks.pre, function (state, key) {
    //         it('should return code as when defaultBlock is 0 at 0x' + key, function () {
    //             asyncTest(['0x' + key, '0x0'], state.code);
    //         });
    //     });

    //     _.each(config.testBlocks.pre, function (state, key) {
    //         if (state.code === '0x') {
    //             it('should return nothing as there is no code at block 0', function () {
    //                 asyncTest(['0x' + key, '0x0'], '0x');
    //             });
    //         }
    //     });

    it('should return an error when no parameter is passed', async function () {
        await asyncErrorTest();
    });
});
