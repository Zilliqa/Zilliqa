const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_getBlockByNumber';

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", async function () {
    before(async function () {
     // if (!helper.isZilliqaNetworkSelected()) {
     //   this.skip();
     // } enable later?
    })

    it("should return an error when called with no parameters", async function () {
      await helper.callEthMethod(METHOD, 1, [],
        (result, status) => {
          console.log(result);
          assert.equal(status, 200, 'has status code');

          assert.isNumber(result.error.code, "Is not a number");
          assert.equal(+result.error.code, -32602);
          assert.isString(result.error.message, 'is string');
          assert.equal(result.error.message, 'INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised');
        })
    })

    it("should return an error when called with only first parameter", async function () {
      await helper.callEthMethod(METHOD, 1, ["latest"],
        (result, status) => {
          console.log(result);
          assert.equal(status, 200, 'has status code');

          assert.isNumber(result.error.code, "Is not a number");
          assert.equal(+result.error.code, -32602);
          assert.isString(result.error.message, 'is string');
          assert.equal(result.error.message, 'INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised');
        })
    })

    it("should return get full transactions objects by 'latest' tag", async function () {
      await helper.callEthMethod(METHOD, 2, ["latest", true],
        (result, status) => {
          console.log(result);

          assert.equal(status, 200, 'has status code');

          assert.property(result, 'result', (result.error) ? result.error.message : 'error');
          assert.isObject(result.result, 'is not an object');

          // difficulty
          assert.match(result.result.difficulty, /^0x/, 'Should be HEX starting with 0x');
          assert.isNumber(+result.result.difficulty, 'Is not a number');
          // totalDifficulty
          assert.match(result.result.totalDifficulty, /^0x/, 'Should be HEX starting with 0x');
          assert.isNumber(+result.result.totalDifficulty, 'Is not a number');
        })
    })
  })
})