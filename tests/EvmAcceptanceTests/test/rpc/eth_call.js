const helper = require('../../helper/GeneralHelper');
const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
const { hardhat } = require("hardhat");
assert = require('chai').assert;

const METHOD = 'eth_call';
var zHelper = new ZilliqaHelper();

// need to test:
// - contracts call with parameters, check result etc...
// - contracts with revert
// ????

describe("Calling " + METHOD, function () {
  it("should return the from eth call", async function () {

    console.log(await hre.artifacts.getArtifactPaths());
    const ContractRaw = await hre.artifacts.readArtifact("contracts/Storage.sol:Storage");

    console.log(ContractRaw.bytecode);

    await helper.callEthMethod(METHOD, 2, [{
      "to": zHelper.getPrimaryAccount().address,
      "data": ContractRaw.bytecode,
      "gas": 1000000
      //,      "value": 4200
    }, "latest"],
      (result, status) => {
        console.log(result);
        assert.equal(status, 200, 'has status code');
        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
        assert.isString(result.result, 'is string');
        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
        assert.isNumber(+result.result, 'can be converted to a number');

        //const expectedChainId = helper.getEthChainId()
        //assert.equal(+result.result, expectedChainId, 'should have a chain Id ' + expectedChainId);
      })
  })
})