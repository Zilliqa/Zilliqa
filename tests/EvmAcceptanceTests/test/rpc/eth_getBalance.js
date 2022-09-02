const { assert } = require('chai')
const Utils = require('../../helper/Utils')

// METHOD
var method = 'eth_getBalance';


// TEST
var asyncTest = async function(params, expectedResult){
    await Utils.callEthMethod(method, 1, params, function(result, status) {
        console.log(result)
        
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        assert.equal(+result.result, expectedResult, 'should have balance '+ expectedResult);
        console.log(result)
    });
};

var asyncErrorTest = async function(){
    await Utils.callEthMethod(method, 1, [],
        function(result, status) {
            assert.equal(status, 200, 'has status code');
            assert.property(result, 'error');
            assert.equal(result.error.code, -32602);
        });
};

describe(method, async function () {
    const key = "F0C05464f12cB2a011d21dE851108a090C95c755"
    it('should return the correct balance at defaultBlock "latest" at address 0x' + key, async function () {
        await asyncTest(['0x' + key, 'latest'], 98475700180000000000)
    });

    it('should return the correct balance at defaultBlock 0 at address 0x' + key, async function () {
        await asyncTest(['0x' + key, '0x0'], 0);
    });

    it('should return an error when no parameter is passed', async function () {
        await asyncErrorTest();
    });
});
