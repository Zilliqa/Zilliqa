import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_getTransactionReceipt";

describe(`Calling ${METHOD} #parallel`, function () {
  before(async function () {
    if (!hre.isZilliqaNetworkSelected()) {
      this.skip();
    }
  });

  it("should return the raw transaction response @block-1", async function () {
    let amount = 10_000;
    // send amount from primary to secondary account
    const to = ethers.Wallet.createRandom();
    const signer = hre.allocateEthSigner();
    const response = await signer.sendTransaction({
      to: to.address,
      value: amount
    });
    hre.releaseEthSigner(signer);
    const transactionHash = response.hash;
    await response.wait();

    await sendJsonRpcRequest(METHOD, 1, [transactionHash], (result, status) => {
      logDebug(result);

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
        to.address.toUpperCase(),
        "Is not equal to " + to.address.toUpperCase()
      );

      // from
      assert.isString(result.result.from, "Is not a string");
      assert.match(result.result.from, /^0x/, "Should be HEX starting with 0x");
      assert.equal(
        result.result.from.toUpperCase(),
        signer.address.toUpperCase(),
        "Is not equal to " + signer.address.toUpperCase()
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
