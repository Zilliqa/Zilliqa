const helper = require('../../helper/GeneralHelper');
const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
assert = require('chai').assert;
const { ethers, web3 } = require("hardhat")

const METHOD = 'eth_getTransactionCount';
let zHelper = new ZilliqaHelper();

describe("Calling " + METHOD, function () {
  // Test that we get no error and that the api call returns a transaction count >= 0.
  it("Should return the latest transaction count >= 0", async function () {
    await helper.callEthMethod(METHOD, 1, [zHelper.getPrimaryAccountAddress(), "latest"],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedTransactionCount = 0;
        assert.isAtLeast(+result.result, expectedTransactionCount, 'should have a transaction count >=:' + expectedTransactionCount);
      })
  })

  it("Should return the pending transaction count >= 0", async function () {
    await helper.callEthMethod(METHOD, 1, [zHelper.getPrimaryAccountAddress(), "pending"],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedTransactionCount = 0;
        assert.isAtLeast(+result.result, expectedTransactionCount, 'should have a transaction count >=:' + expectedTransactionCount);
      })
  })

  it("Should return the earliest transaction count >= 0", async function () {
    await helper.callEthMethod(METHOD, 1, [zHelper.getPrimaryAccountAddress(), "earliest"],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        const expectedTransactionCount = 0;
        assert.isAtLeast(+result.result, expectedTransactionCount, 'should have a transaction count >=:' + expectedTransactionCount);
      })
  })
})