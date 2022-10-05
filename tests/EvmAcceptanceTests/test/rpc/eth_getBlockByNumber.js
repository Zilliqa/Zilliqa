const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "eth_getBlockByNumber";

describe("Calling " + METHOD, function () {
  function TestResponse(response) {
    // validate all returned fields

    assert.property(response, "result", response.error ? response.error.message : "error");
    assert.isObject(response.result, "is not an object");

    // difficulty
    assert.match(response.result.difficulty, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.difficulty, "Is not a number");
    // totalDifficulty
    assert.match(response.result.totalDifficulty, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.totalDifficulty, "Is not a number");
    // extraData
    assert.match(response.result.extraData, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.extraData, "Is not a number");
    // gasLimit
    assert.match(response.result.gasLimit, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.gasLimit, "Is not a number");
    // gasUsed
    assert.match(response.result.gasUsed, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.gasUsed, "Is not a number");
    // hash
    assert.match(response.result.hash, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.hash, "Is not a number");
    // parentHash
    assert.match(response.result.parentHash, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.parentHash, "Is not a number");
    // logsBloom
    assert.match(response.result.logsBloom, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.logsBloom, "Is not a number");
    // miner
    assert.match(response.result.miner, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.miner, "Is not a number");
    // number
    assert.match(response.result.number, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.number, "Is not a number");
    // sha3Uncles
    assert.match(response.result.sha3Uncles, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.sha3Uncles, "Is not a number");
    // size
    assert.match(response.result.size, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.size, "Is not a number");
    // stateRoot
    assert.match(response.result.stateRoot, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.stateRoot, "Is not a number");
    // timestamp
    assert.match(response.result.timestamp, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.timestamp, "Is not a number");
    // nonce
    assert.match(response.result.nonce, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.nonce, "Is not a number");
    // receiptsRoot
    assert.match(response.result.receiptsRoot, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.receiptsRoot, "Is not a number");
    // transactionsRoot
    assert.match(response.result.transactionsRoot, /^0x/, "Should be HEX starting with 0x");
    assert.isNumber(+response.result.transactionsRoot, "Is not a number");
    // transactions
    assert.isArray(response.result.transactions, "Is not an array");
    // uncles
    assert.isArray(response.result.uncles, "Is not an array");
  }

  it("should return an error when called with no parameters", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      hre.logDebug(result);

      assert.equal(status, 200, "has status code");

      assert.isNumber(result.error.code, "Is not a number");
      assert.equal(+result.error.code, -32602);
      assert.isString(result.error.message, "is string");
      assert.equal(
        result.error.message,
        "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised"
      );
    });
  });

  it("should return an error when called with only first parameter", async function () {
    await helper.callEthMethod(METHOD, 1, ["latest"], (result, status) => {
      hre.logDebug(result);
      assert.equal(status, 200, "has status code");

      assert.isNumber(result.error.code, "Is not a number");
      assert.equal(+result.error.code, -32602);
      assert.isString(result.error.message, "is string");
      assert.equal(
        result.error.message,
        "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised"
      );
    });
  });

  it("should return get full transactions objects by 'unknown tag' tag", async function () {
    await helper.callEthMethod(METHOD, 2, ["unknown tag", true], (result, status) => {
      hre.logDebug(result);

      assert.equal(status, 200, "has status code");
      assert.equal(result.result, null, "should be null");
    });
  });

  it("should return get full transactions objects by 'latest' tag", async function () {
    await helper.callEthMethod(METHOD, 2, ["latest", true], (result, status) => {
      hre.logDebug(result);

      assert.equal(status, 200, "has status code");
      // validate all returned fields
      TestResponse(result);
    });
  });

  it("should return get only the hashes of the transactions by 'latest' tag", async function () {
    await helper.callEthMethod(METHOD, 2, ["latest", false], (result, status) => {
      hre.logDebug(result);

      assert.equal(status, 200, "has status code");
      // validate all returned fields
      TestResponse(result);
    });
  });

  //it("should return get full transactions objects by 'earliest' tag", async function () {
  //  await helper.callEthMethod(METHOD, 2, ["earliest", true], (result, status) => {
  //    hre.logDebug(result);

  //    assert.equal(status, 200, "has status code");
  //    // validate all returned fields
  //    TestResponse(result);
  //    assert.equal(+result.result.number, 0, "Block number is not '0'");
  //  });
  //});

  //it("should return get only the hashes of the transactions objects by 'earliest' tag", async function () {
  //  await helper.callEthMethod(METHOD, 2, ["earliest", false], (result, status) => {
  //    hre.logDebug(result);

  //    assert.equal(status, 200, "has status code");
  //    // validate all returned fields
  //    TestResponse(result);
  //    assert.equal(+result.result.number, 0, "Block number is not '0'");
  //  });
  //});

  //it("should return get full transactions objects by its block number '0'", async function () {
  //  await helper.callEthMethod(METHOD, 2, ["0x0", true], (result, status) => {
  //    hre.logDebug(result);

  //    assert.equal(status, 200, "has status code");
  //    // validate all returned fields

  //    TestResponse(result);
  //    assert.equal(+result.result.number, 0, "Block number is not '0'");
  //  });
  //});

  //it("should return get only the hashes of transactions objects by its block number '0'", async function () {
  //  await helper.callEthMethod(METHOD, 2, ["0x0", false], (result, status) => {
  //    hre.logDebug(result);

  //    assert.equal(status, 200, "has status code");
  //    // validate all returned fields

  //    TestResponse(result);
  //    assert.equal(+result.result.number, 0, "Block number is not '0'");
  //  });
  //});

  describe("When on Zilliqa network", function () {
    before(function () {
      if (!helper.isZilliqaNetworkSelected()) {
        this.skip();
      }
    });
    it("should return get full transactions objects by 'pending' tag", async function () {
      await helper.callEthMethod(METHOD, 2, ["pending", true], (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        // validate all returned fields

        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.equal(result.result, null, "should be null");
      });
    });
  });
});
