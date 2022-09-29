const helper = require('../../helper/GeneralHelper');
const zilliqa_helper = require("../../helper/ZilliqaHelper");
const { hardhat } = require("hardhat");
assert = require('chai').assert;

const METHOD = 'eth_call';

describe("Calling " + METHOD, function () {
  it("should return the from eth call", async function () {

    const contractFactory = await hre.ethers.getContractFactory("SimpleContract");
    const ContractRaw = await hre.artifacts.readArtifact("contracts/SimpleContract.sol:SimpleContract");

    await helper.callEthMethod(METHOD, 2, [{
      "to": zilliqa_helper.getPrimaryAccountAddress(),
      "data": ContractRaw.bytecode,
      "gas": 1000000
    }, "latest"],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');
        assert.equal(result.result, "0x");
      })
  })
})