const helper = require('../../helper/GeneralHelper');
assert = require('chai').assert;

const METHOD = 'eth_getBlockByNumber';

describe("Calling " + METHOD, function () {


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
        // validate all returned fields

        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isObject(result.result, 'is not an object');

        // difficulty
        assert.match(result.result.difficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.difficulty, 'Is not a number');

        // totalDifficulty
        assert.match(result.result.totalDifficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.totalDifficulty, 'Is not a number');

        // extraData
        assert.match(result.result.extraData, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.extraData, 'Is not a number');

        // gasLimit
        assert.match(result.result.gasLimit, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasLimit, 'Is not a number');

        // gasUsed
        assert.match(result.result.gasUsed, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasUsed, 'Is not a number');

        // hash
        assert.match(result.result.hash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.hash, 'Is not a number');

        // parentHash
        assert.match(result.result.parentHash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.parentHash, 'Is not a number');

        // logsBloom
        assert.match(result.result.logsBloom, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.logsBloom, 'Is not a number');

        // miner
        assert.match(result.result.miner, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.miner, 'Is not a number');

        // number
        assert.match(result.result.number, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.number, 'Is not a number');

        // sha3Uncles
        assert.match(result.result.sha3Uncles, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.sha3Uncles, 'Is not a number');

        // size
        assert.match(result.result.size, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.size, 'Is not a number');

        // stateRoot
        assert.match(result.result.stateRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.stateRoot, 'Is not a number');

        // timestamp
        assert.match(result.result.timestamp, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.timestamp, 'Is not a number');

        // nonce
        assert.match(result.result.nonce, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.nonce, 'Is not a number');

        // receiptsRoot
        assert.match(result.result.receiptsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.receiptsRoot, 'Is not a number');

        // transactionsRoot
        assert.match(result.result.transactionsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.transactionsRoot, 'Is not a number');

        // transactions
        assert.isArray(result.result.transactions, 'Is not an array');

        // uncles
        assert.isArray(result.result.uncles, 'Is not an array');
      })
  })


  it("should return transactions hashes by 'latest' tag", async function () {
    await helper.callEthMethod(METHOD, 2, ["latest", false],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        // validate all returned fields

        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isObject(result.result, 'is not an object');

        // difficulty
        assert.match(result.result.difficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.difficulty, 'Is not a number');

        // totalDifficulty
        assert.match(result.result.totalDifficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.totalDifficulty, 'Is not a number');

        // extraData
        assert.match(result.result.extraData, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.extraData, 'Is not a number');

        // gasLimit
        assert.match(result.result.gasLimit, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasLimit, 'Is not a number');

        // gasUsed
        assert.match(result.result.gasUsed, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasUsed, 'Is not a number');

        // hash
        assert.match(result.result.hash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.hash, 'Is not a number');

        // parentHash
        assert.match(result.result.parentHash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.parentHash, 'Is not a number');

        // logsBloom
        assert.match(result.result.logsBloom, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.logsBloom, 'Is not a number');

        // miner
        assert.match(result.result.miner, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.miner, 'Is not a number');

        // number
        assert.match(result.result.number, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.number, 'Is not a number');

        // sha3Uncles
        assert.match(result.result.sha3Uncles, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.sha3Uncles, 'Is not a number');

        // size
        assert.match(result.result.size, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.size, 'Is not a number');

        // stateRoot
        assert.match(result.result.stateRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.stateRoot, 'Is not a number');

        // timestamp
        assert.match(result.result.timestamp, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.timestamp, 'Is not a number');

        // nonce
        assert.match(result.result.nonce, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.nonce, 'Is not a number');

        // receiptsRoot
        assert.match(result.result.receiptsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.receiptsRoot, 'Is not a number');

        // transactionsRoot
        assert.match(result.result.transactionsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.transactionsRoot, 'Is not a number');

        // transactions
        assert.isArray(result.result.transactions, 'Is not an array');

        // uncles
        assert.isArray(result.result.uncles, 'Is not an array');
      })
  })

  it("should return get full transactions objects by 'earliest' tag", async function () {
    await helper.callEthMethod(METHOD, 2, ["earliest", true],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        // validate all returned fields

        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isObject(result.result, 'is not an object');

        // difficulty
        assert.match(result.result.difficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.difficulty, 'Is not a number');

        // totalDifficulty
        assert.match(result.result.totalDifficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.totalDifficulty, 'Is not a number');

        // extraData
        assert.match(result.result.extraData, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.extraData, 'Is not a number');

        // gasLimit
        assert.match(result.result.gasLimit, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasLimit, 'Is not a number');

        // gasUsed
        assert.match(result.result.gasUsed, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasUsed, 'Is not a number');

        // hash
        assert.match(result.result.hash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.hash, 'Is not a number');

        // parentHash
        assert.match(result.result.parentHash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.parentHash, 'Is not a number');

        // logsBloom
        assert.match(result.result.logsBloom, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.logsBloom, 'Is not a number');

        // miner
        assert.match(result.result.miner, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.miner, 'Is not a number');

        // number
        assert.match(result.result.number, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.number, 'Is not a number');

        // sha3Uncles
        assert.match(result.result.sha3Uncles, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.sha3Uncles, 'Is not a number');

        // size
        assert.match(result.result.size, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.size, 'Is not a number');

        // stateRoot
        assert.match(result.result.stateRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.stateRoot, 'Is not a number');

        // timestamp
        assert.match(result.result.timestamp, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.timestamp, 'Is not a number');

        // nonce
        assert.match(result.result.nonce, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.nonce, 'Is not a number');

        // receiptsRoot
        assert.match(result.result.receiptsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.receiptsRoot, 'Is not a number');

        // transactionsRoot
        assert.match(result.result.transactionsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.transactionsRoot, 'Is not a number');

        // transactions
        assert.isArray(result.result.transactions, 'Is not an array');

        // uncles
        assert.isArray(result.result.uncles, 'Is not an array');
      })
  })


  it("should return get full transactions objects by its block number", async function () {
    await helper.callEthMethod(METHOD, 2, ["0x0", true],
      (result, status) => {
        console.log(result);

        assert.equal(status, 200, 'has status code');
        // validate all returned fields

        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isObject(result.result, 'is not an object');

        // difficulty
        assert.match(result.result.difficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.difficulty, 'Is not a number');

        // totalDifficulty
        assert.match(result.result.totalDifficulty, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.totalDifficulty, 'Is not a number');

        // extraData
        assert.match(result.result.extraData, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.extraData, 'Is not a number');

        // gasLimit
        assert.match(result.result.gasLimit, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasLimit, 'Is not a number');

        // gasUsed
        assert.match(result.result.gasUsed, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.gasUsed, 'Is not a number');

        // hash
        assert.match(result.result.hash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.hash, 'Is not a number');

        // parentHash
        assert.match(result.result.parentHash, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.parentHash, 'Is not a number');

        // logsBloom
        assert.match(result.result.logsBloom, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.logsBloom, 'Is not a number');

        // miner
        assert.match(result.result.miner, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.miner, 'Is not a number');

        // number
        assert.match(result.result.number, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.number, 'Is not a number');

        // sha3Uncles
        assert.match(result.result.sha3Uncles, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.sha3Uncles, 'Is not a number');

        // size
        assert.match(result.result.size, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.size, 'Is not a number');

        // stateRoot
        assert.match(result.result.stateRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.stateRoot, 'Is not a number');

        // timestamp
        assert.match(result.result.timestamp, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.timestamp, 'Is not a number');

        // nonce
        assert.match(result.result.nonce, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.nonce, 'Is not a number');

        // receiptsRoot
        assert.match(result.result.receiptsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.receiptsRoot, 'Is not a number');

        // transactionsRoot
        assert.match(result.result.transactionsRoot, /^0x/, 'Should be HEX starting with 0x');
        assert.isNumber(+result.result.transactionsRoot, 'Is not a number');

        // transactions
        assert.isArray(result.result.transactions, 'Is not an array');

        // uncles
        assert.isArray(result.result.uncles, 'Is not an array');
      })
  })


  it("should return get full transactions objects by 'pending' tag", async function () {
    describe("When on Zilliqa network", async function () {
      before(async function () {
        if (!helper.isZilliqaNetworkSelected()) {
          this.skip();
        }
      })

      await helper.callEthMethod(METHOD, 2, ["pending", true],
        (result, status) => {
          console.log(result);

          assert.equal(status, 200, 'has status code');
          // validate all returned fields

          assert.property(result, 'result', (result.error) ? result.error.message : 'error');
          assert.equal(result.result, null, 'should be null');
        })
    })
  })
})