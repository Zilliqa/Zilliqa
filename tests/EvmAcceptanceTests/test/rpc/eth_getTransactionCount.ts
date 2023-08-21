import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_getTransactionCount";

describe(`Calling ${METHOD} #parallel`, function () {
  // Test that we get no error and that the api call returns a transaction count >= 0.
  it("Should return the latest transaction count >= 0 @block-1", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "latest"], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedTransactionCount = 0;
      assert.isAtLeast(
        +result.result,
        expectedTransactionCount,
        "should have a transaction count >=:" + expectedTransactionCount
      );
    });
  });

  it("Should return the pending transaction count >= 0 @block-1", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "pending"], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedTransactionCount = 0;
      assert.isAtLeast(
        +result.result,
        expectedTransactionCount,
        "should have a transaction count >=:" + expectedTransactionCount
      );
    });
  });

  it("Should return the earliest transaction count >= 0", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "earliest"], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      const expectedTransactionCount = 0;
      assert.isAtLeast(
        +result.result,
        expectedTransactionCount,
        "should have a transaction count >=:" + expectedTransactionCount
      );
    });
  });
});
