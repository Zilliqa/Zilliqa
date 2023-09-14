import {assert, expect} from "chai";
import hre, {ethers} from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {Contract} from "ethers";

const METHOD = "ots_traceTransaction";
describe(`Otterscan api tests: ${METHOD} #parallel`, function () {
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;
  before(async function () {
    const METHOD_ENABLE = "ots_enable";

    // Make sure tracing is enabled
    await sendJsonRpcRequest(METHOD_ENABLE, 1, [true], (result, status) => {
      assert.equal(status, 200, "has status code");
    });

    if (hre.parallel) {
      [contractOne, contractTwo, contractThree] = await Promise.all([
        await hre.deployContract("ContractOne"),
        await hre.deployContract("ContractTwo"),
        await hre.deployContract("ContractThree")
      ]);
    } else {
      contractOne = await hre.deployContract("ContractOne");
      contractTwo = await hre.deployContract("ContractTwo");
      contractThree = await hre.deployContract("ContractThree");
    }
  });

  it("We can get the otter trace transaction @block-1", async function () {
    let addrOne = contractOne.address.toLowerCase();
    let addrTwo = contractTwo.address.toLowerCase();
    let addrThree = contractThree.address.toLowerCase();

    let res = await contractOne.chainedCall([addrTwo, addrThree, addrOne], 0);

    await sendJsonRpcRequest(METHOD, 1, [res.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject[0]["depth"], 0, "has correct depth initially");
      assert.equal(jsonObject[1]["depth"], 1, "has correct depth one call down");
      assert.equal(jsonObject[1]["type"], "CALL", "has correct depth one call down");
    });
  });
});
