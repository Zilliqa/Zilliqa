import { assert } from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
const eip1898TestParams = require('../../common/eip1898TestParams');


// Now you can use the parameters from the imported object
const {
  validEip1898Complete,
  validEip1898WithHash,
  validEip1898WithNumber,
  invalidEip1898Empty,
  invalidEip1898IncorrectKeys
} = eip1898TestParams;


const METHOD = "eth_getCode";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return an error when no parameter is passed @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      logDebug(result, status);
      assert.equal(status, 200, "has status code");
      assert.property(result, "error");
      assert.equal(result.error.code, -32602);
    });
  });

  it("should return code of contract @block-1", async function () {
    const contract = await hre.deployContract("SimpleContract");
    const expected = hre.artifacts.readArtifactSync("SimpleContract").deployedBytecode;

    await sendJsonRpcRequest(METHOD, 1, [contract.address, "latest"], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(result.result.toLowerCase(), expected.toLowerCase(), "should be " + expected);
    });
  });

  it("should return code of contract for eip-1898 format request scenario1 @block-1", async function () {
    const contract = await hre.deployContract("SimpleContract");
    const expected = hre.artifacts.readArtifactSync("SimpleContract").deployedBytecode;

    await sendJsonRpcRequest(METHOD, 1, [contract.address, validEip1898Complete], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(result.result.toLowerCase(), expected.toLowerCase(), "should be " + expected);
    });
  });
  it("should return code of contract for eip-1898 format request scenario2 @block-1", async function () {
    const contract = await hre.deployContract("SimpleContract");
    const expected = hre.artifacts.readArtifactSync("SimpleContract").deployedBytecode;

    await sendJsonRpcRequest(METHOD, 1, [contract.address, validEip1898WithHash], (result, status) => {
      logDebug(result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(result.result.toLowerCase(), expected.toLowerCase(), "should be " + expected);
    });
  });
});
