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
    const key = "bF0476C48C0E5353983c372738006a122ec50E8b"
    it('should return the correct balance at defaultBlock "latest" at address 0x' + key, async function () {
        const expectedResult = 0xDE0B6B3A7640000 // should be 1 eth in wei //// todo calculate balance from 1 / 10^18 in a bignumber
        await asyncTest(['0x' + key, 'latest'], expectedResult)
    });

    it('should return the correct balance at defaultBlock 0 at address 0x' + key, async function () {
        await asyncTest(['0x' + key, '0x0'], 0);
    });

    it('should return an error when no parameter is passed', async function () {
        await asyncErrorTest();
    });
});
