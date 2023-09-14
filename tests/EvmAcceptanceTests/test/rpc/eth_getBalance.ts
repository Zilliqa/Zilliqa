import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_getBalance";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return the latest balance from the specified account #parallel", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "latest"], (result, status) => {
      logDebug("Result:", result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      var expectedBalance = 0;
      assert.isAbove(
        +result.result,
        expectedBalance,
        "Has result:" + result + " should have balance " + expectedBalance
      );
    });
  });

  it("should return the earliest balance as specified in the ethereum protocol @block-1", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "earliest"], (result, status) => {
      logDebug("Result:", result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      var expectedBalance = 0;
      assert.isAbove(
        +result.result,
        expectedBalance,
        "Has result:" + result + " should have balance " + expectedBalance
      );
    });
  });

  it("should return the pending balance as specified in the ethereum protocol @block-1", async function () {
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(METHOD, 1, [signer.address, "pending"], (result, status) => {
      logDebug("Result:", result);

      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.isNumber(+result.result, "can be converted to a number");

      var expectedBalance = 0;
      assert.isAbove(
        +result.result,
        expectedBalance,
        "Has result:" + result + " should have balance " + expectedBalance
      );
    });
  });

  it("should return an error requesting the balance due to invalid tag @block-1", async function () {
    let expectedErrorMessage = "Unable To Process, invalid tag";
    let errorCode = -1;
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(
      METHOD,
      1,
      [signer.address, "unknown tag"], // not supported tag should give an error
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.equal(result.error.code, errorCode);
        assert.equal(result.error.message, expectedErrorMessage);
      }
    );
  });

  it("should return an error requesting the balance due to insufficient parameters @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised";
    let errorCode = -32602;
    const [signer] = await ethers.getSigners();
    await sendJsonRpcRequest(
      METHOD,
      1,
      [signer.address], // insufficient parameters
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.equal(result.error.code, errorCode);
        assert.equal(result.error.message, expectedErrorMessage);
      }
    );
  });

  it("should return an error requesting the balance if no parameters is specified @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised";
    let errorCode = -32602;
    await sendJsonRpcRequest(
      METHOD,
      1,
      [], // insufficient parameters
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.equal(result.error.code, errorCode);
        assert.equal(result.error.message, expectedErrorMessage);
      }
    );
  });
});
