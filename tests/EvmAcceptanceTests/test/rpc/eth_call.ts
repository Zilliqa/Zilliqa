import callJsonRpc from "../../helpers/JsonRpcHelper";
import { assert } from "chai";
import { ethers } from "hardhat";
import logDebug from "../../helpers/DebugHelper";
const eip1898TestParams = require('../../common/eip1898TestParams');


// Now you can use the parameters from the imported object
const {
  validEip1898Complete,
  validEip1898WithHash,
  validEip1898WithNumber,
  invalidEip1898Empty,
  invalidEip1898IncorrectKeys
} = eip1898TestParams;

const METHOD = "eth_call";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return the from eth call @block-1", async function () {
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "latest"
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });
  it("should return the from eth call when input field is set @block-1", async function () {
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          input: contractFactory.bytecode,
          gas: 1000000
        },
        "latest"
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });
  it("should return the from eth call as per eip-1898 @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised";
    let errorCode = -32602;
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          data: contractFactory.bytecode,
          gas: 1000000
        },
        validEip1898Complete
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }

    );
  });
  it("should throw error for invalid eth call as per eip-1898 scenario1 @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised: blockParam object must contain either blockHash or blockNumber or both";
    let errorCode = -32602;
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          data: contractFactory.bytecode,
          gas: 1000000
        },
        invalidEip1898IncorrectKeys
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.equal(result.error.code, errorCode);
        assert.equal(result.error.message, expectedErrorMessage);
      }

    );
  });
  it("should throw error for invalid eth call as per eip-1898 scenario2 @block-1", async function () {
    let expectedErrorMessage = "INVALID_PARAMS: Invalid method parameters (invalid name and/or type) recognised: blockParam object must contain either blockHash or blockNumber or both";
    let errorCode = -32602;
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          data: contractFactory.bytecode,
          gas: 1000000
        },
        invalidEip1898Empty
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.equal(result.error.code, errorCode);
        assert.equal(result.error.message, expectedErrorMessage);
      }

    );
  });
});
