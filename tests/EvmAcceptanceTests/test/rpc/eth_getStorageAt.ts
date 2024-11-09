import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import { assert } from "chai";
import hre, { web3 } from "hardhat";
import logDebug from "../../helpers/DebugHelper";
import { Contract } from "ethers";
const eip1898TestParams = require('../../common/eip1898TestParams');


// Now you can use the parameters from the imported object
const {
  validEip1898Complete,
  validEip1898WithHash,
  validEip1898WithNumber,
  invalidEip1898Empty,
  invalidEip1898IncorrectKeys
} = eip1898TestParams;

var METHOD = "eth_getStorageAt";

describe(`Calling ${METHOD} #parallel`, function () {
  let contract: Contract;
  before(async function () {
    contract = await hre.deployContract("Storage");
  });

  it("should return proper storage value when the defaultBlock is 'latest' for storage position 0 at address of contract @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [contract.address, "0x0", "latest"], (result, status) => {
      logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 1024);
    });
  });

  it("should return proper storage value when a value from a map is requested @block-1", async function () {
    const MAPPING_SLOT = "0000000000000000000000000000000000000000000000000000000000000001";

    // KEY that we want to read in the mapping
    const KEY = "0000000000000000000000000000000000000000000000000000000000000010";

    // Compute the actual storage slot of the value associated with the key
    const balanceSlot = web3.utils.soliditySha3({ t: "bytes", v: KEY + MAPPING_SLOT });

    await sendJsonRpcRequest(METHOD, 1, [contract.address, balanceSlot, "latest"], (result, status) => {
      logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 2048);
    });
  });

  it("should return proper storage value when the defaultBlock is '0x0' for storage position 0 at address of contract @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [contract.address, "0x0", "0x0"], (result, status) => {
      logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 1024);
    });
  });

  it("should return an error when no parameter is passed @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
      assert.equal(status, 200, "has status code");
      assert.property(result, "error");
      assert.equal(result.error.code, -32602);
    });
  });
  it("should return proper storage value when the defaultBlock is '0x0' for storage position 0 at address of contract as per eip-1898 @block-1", async function () {
    await sendJsonRpcRequest(METHOD, 1, [contract.address, "0x0", validEip1898Complete], (result, status) => {
      logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 1024);
    });
  });
  it("should return error if invalid eip-1898 parameters are passed @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised: blockParam object must contain either blockHash or blockNumber or both";
    let errorCode = -32602;
    await sendJsonRpcRequest(METHOD, 1, [contract.address, "0x0", invalidEip1898Empty], (result, status) => {
      logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.equal(result.error.code, errorCode);
      assert.equal(result.error.message, expectedErrorMessage);
    });
  });
});
