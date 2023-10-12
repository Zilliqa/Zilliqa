import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre, {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_getTransactionByHash";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should have valid structure in response", async function () {
    const to = ethers.Wallet.createRandom();
    const {response, signer_address} = await hre.sendEthTransaction({
      to: to.address,
      value: 1_000_000
    });
    const transactionHash = response.hash;

    await sendJsonRpcRequest(METHOD, 2, [transactionHash], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");

      // gas
      assert.isString(result.result.gas, "Is not a string");
      assert.match(result.result.gas, /^0x/, "Should be HEX starting with 0x");

      // gasLimit
      assert.isString(result.result.gasLimit, "Is not a string");
      assert.match(result.result.gasLimit, /^0x/, "Should be HEX starting with 0x");

      // gasPrice
      assert.isString(result.result.gasPrice, "Is not a string");
      assert.match(result.result.gasPrice, /^0x/, "Should be HEX starting with 0x");

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
        signer_address.toUpperCase(),
        "Is not equal to " + signer_address.toUpperCase()
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
      assert.isString(result.result.hash, "Is not a string");
      assert.match(result.result.hash, /^0x/, "Should be HEX starting with 0x");
      assert.equal(
        result.result.hash.toUpperCase(),
        transactionHash.toUpperCase(),
        "Is not equal to " + transactionHash.toUpperCase()
      );

      // value
      assert.isString(result.result.value, "Is not a string");
      assert.match(result.result.value, /^0x/, "Should be HEX starting with 0x");
      assert.equal(parseInt(result.result.value, 16), 1_000_000, "Is not equal to " + 1_000_000);
    });
  });

  it("should return null when no transaction was found @block-1", async function () {
    const INVALID_HASH = "0xd2f1575105fd2272914d77355b8dab5afbdde4b012abd849e8b32111be498b0d";
    await sendJsonRpcRequest(METHOD, 1, [INVALID_HASH], (result, status) => {
      logDebug(result, status);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isNull(result.result);
    });
  });

  it("should return an error when no parameter is passed @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result, status);
      assert.equal(status, 200, "has status code");
      assert.property(result, "error");
      assert.equal(result.error.code, -32602);
    });
  });
});
