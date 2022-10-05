const helper = require("../../helper/GeneralHelper");
const zilliqa_helper = require("../../helper/ZilliqaHelper");
assert = require("chai").assert;

const METHOD = "eth_call";

describe("Calling " + METHOD, function () {
  it("should perform eth call on the 'latest' block", async function () {
    const contractFactory = await hre.ethers.getContractFactory("SimpleContract");
    await helper.callEthMethod(
      METHOD,
      2,
      [
        {
          to: zilliqa_helper.getPrimaryAccountAddress(),
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "latest"
      ],
      (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });

  it("should perform eth call on the 'pending' block", async function () {
    const contractFactory = await hre.ethers.getContractFactory("SimpleContract");
    await helper.callEthMethod(
      METHOD,
      2,
      [
        {
          to: zilliqa_helper.getPrimaryAccountAddress(),
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "pending"
      ],
      (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });
  it("should perform eth call on the 'earliest' block", async function () {
    const contractFactory = await hre.ethers.getContractFactory("SimpleContract");
    await helper.callEthMethod(
      METHOD,
      2,
      [
        {
          to: zilliqa_helper.getPrimaryAccountAddress(),
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "earliest"
      ],
      (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });

  it("should perform eth call on a block number", async function () {
    const contractFactory = await hre.ethers.getContractFactory("SimpleContract");
    await helper.callEthMethod(
      METHOD,
      2,
      [
        {
          to: zilliqa_helper.getPrimaryAccountAddress(),
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "0x0"
      ],
      (result, status) => {
        hre.logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });

});
