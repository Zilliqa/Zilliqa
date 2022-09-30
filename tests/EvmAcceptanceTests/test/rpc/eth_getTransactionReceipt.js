const zilliqa_helper = require("../../helper/ZilliqaHelper");
const helper = require("../../helper/GeneralHelper");
assert = require("chai").assert;

const METHOD = "eth_getTransactionReceipt";

describe("Calling " + METHOD, function () {
  before(async function () {
    if (!helper.isZilliqaNetworkSelected()) {
      this.skip();
    }
  });

  it("should return the raw transaction response", async function () {
    var transactionHash;

    function onMoveFundsFinished(receipt) {
      if (hre.debugMode) {
        console.log("Moved funds successfully, receipt:", receipt);
      }
      transactionHash = receipt.transactionHash;
    }

    function onMoveFundsError(error) {
      if (hre.debugMode) {
        console.log("Then with Error:", error);
      }
      assert.fail("Failure: Unexpected return ", error);
    }

    let amount = 10_000;
    // send amount from primary to secondary account
    await zilliqa_helper
      .moveFundsTo(amount, zilliqa_helper.getSecondaryAccountAddress(), zilliqa_helper.primaryAccount)
      .then(onMoveFundsFinished, onMoveFundsError);

    await helper.callEthMethod(METHOD, 1, [transactionHash], (result, status) => {
      if (hre.debugMode) {
        console.log(result);
      }

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");

      // status
      assert.match(result.result.status, /^0x/, "Should be HEX starting with 0x");
      assert.isString(result.result.status, "Is not a string");
      assert.equal(result.result.status, "0x1", "Expected to be equal to 0x1");

      // root
      assert.isString(result.result.root, "Is not a string");
      assert.match(result.result.root, /^0x/, "Should be HEX starting with 0x");

      // cumulativeGasUsed
      assert.isString(result.result.cumulativeGasUsed, "Is not a string");
      assert.match(result.result.cumulativeGasUsed, /^0x/, "Should be HEX starting with 0x");

      // logsBloom
      assert.isString(result.result.logsBloom, "Is not a string");
      assert.match(result.result.logsBloom, /^0x/, "Should be HEX starting with 0x");

      // logs
      assert.isBoolean(Array.isArray(result.result.logs), "Should be an array");

      // contractAddress
      assert.isBoolean(result.result.contractAddress == null, "Should be HEX starting with 0x");

      // gasUsed
      assert.isString(result.result.gasUsed, "Is not a string");
      assert.match(result.result.gasUsed, /^0x/, "Should be HEX starting with 0x");

      // cumulativeGasUsed
      assert.isString(result.result.cumulativeGasUsed, "Is not a string");
      assert.match(result.result.cumulativeGasUsed, /^0x/, "Should be HEX starting with 0x");

      // to
      assert.isString(result.result.to, "Is not a string");
      assert.match(result.result.to, /^0x/, "Should be HEX starting with 0x");
      assert.equal(
        result.result.to.toUpperCase(),
        zilliqa_helper.getSecondaryAccountAddress().toUpperCase(),
        "Is not equal to " + zilliqa_helper.getSecondaryAccountAddress().toUpperCase()
      );

      // from
      assert.isString(result.result.from, "Is not a string");
      assert.match(result.result.from, /^0x/, "Should be HEX starting with 0x");
      assert.equal(
        result.result.from.toUpperCase(),
        zilliqa_helper.getPrimaryAccountAddress().toUpperCase(),
        "Is not equal to " + zilliqa_helper.getPrimaryAccountAddress().toUpperCase()
      );

      // blockHash
      assert.isString(result.result.blockHash, "Is not a string");
      assert.match(result.result.blockHash, /^0x/, "Should be HEX starting with 0x");

      // blockNumber
      assert.isString(result.result.blockNumber, "Is not a string");
      assert.match(result.result.blockNumber, /^0x/, "Should be HEX starting with 0x");

      // transactionIndex
      assert.isString(result.result.transactionIndex, "Is not a string");
      assert.match(result.result.transactionIndex, /^0x/, "Should be HEX starting with 0x");

      // transactionHash
      assert.isString(result.result.transactionHash, "Is not a string");
      assert.match(result.result.transactionHash, /^0x/, "Should be HEX starting with 0x");
      assert.equal(
        result.result.transactionHash.toUpperCase(),
        transactionHash.toUpperCase(),
        "Is not equal to " + transactionHash.toUpperCase()
      );
    });
  });
});
