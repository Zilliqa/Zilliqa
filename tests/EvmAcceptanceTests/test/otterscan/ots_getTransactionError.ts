import {assert, expect} from "chai";
import hre, {ethers} from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {Contract} from "ethers";

const METHOD = "ots_getTransactionError";
describe(`Otterscan api tests: ${METHOD} #parallel`, function () {
  let revertContract: Contract;
  before(async function () {
    const METHOD_ENABLE = "ots_enable";

    // Make sure tracing is enabled
    await sendJsonRpcRequest(METHOD_ENABLE, 1, [true], (result, status) => {
      assert.equal(status, 200, "has status code");
    });

    revertContract = await hre.deployContract("Revert");
  });

  xit("When we revert the TX, we can get the tx error @block-1", async function () {
    const REVERT_MESSAGE = "Transaction too old";

    const abi = ethers.utils.defaultAbiCoder;
    const MESSAGE_ENCODED = "0x08c379a0" + abi.encode(["string"], [REVERT_MESSAGE]).split("x")[1];

    // In order to make a tx that fails at runtime and not estimate gas time, we estimate the gas of
    // a similar passing call and use this (+30% leeway) to override the gas field
    const estimatedGas = await revertContract.estimateGas.requireCustom(true, REVERT_MESSAGE);

    const tx = await revertContract.requireCustom(false, REVERT_MESSAGE, {gasLimit: estimatedGas.mul(130).div(100)});

    await sendJsonRpcRequest(METHOD, 1, [tx.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject, MESSAGE_ENCODED);
    });
  });
});
