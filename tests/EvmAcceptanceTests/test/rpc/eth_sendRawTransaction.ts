import {assert} from "chai";
import hre from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {web3} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_sendRawTransaction";

describe("Calling " + METHOD, function () {
  it("should return a send raw transaction", async function () {
    const private_keys: string[] = hre.network["config"]["accounts"] as string[];
    const fromAccount = web3.eth.accounts.privateKeyToAccount(private_keys[0]);
    const destination = web3.eth.accounts.create();
    const toAddress = destination.address;
    const nonce = await web3.eth.getTransactionCount(fromAccount.address); // nonce starts counting from 0

    const tx = {
      to: toAddress,
      value: 1_000_000,
      gas: 300000,
      gasPrice: 2000000000000000,
      nonce: nonce,
      chainId: hre.getEthChainId(),
      data: ""
    };

    const signedTx = await fromAccount.signTransaction(tx);

    await sendJsonRpcRequest(METHOD, 1, [signedTx.rawTransaction], (result, status) => {
      logDebug("Result:", result);

      // The result contains a transaction hash that is every time different and should match the hash returned in the result
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.equal(
        result.result,
        signedTx.transactionHash,
        "has result:" + result.result + ", expected transaction hash:" + signedTx.transactionHash
      );
    });
  });

  it("should return transaction hash for a legacy transaction (before EIP-155)", async function () {
    const rawTransaction =
      "0xf8a88088016345785d8a0000830186a08080b853604580600e600039806000f350fe" +
      "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe03601" +
      "600081602082378035828234f58015156039578182fd5b8082525050506014600cf31b" +
      "a02222222222222222222222222222222222222222222222222222222222222222a022" +
      "22222222222222222222222222222222222222222222222222222222222222";

    const transactionHash = "0xdfdd1d0595b01e91fbe2ca1e03cfd5ad7c54fe5323e0bdffb062fe30c522ae14";
    await sendJsonRpcRequest(METHOD, 1, [rawTransaction], (result, status) => {
      logDebug("Result:", result);

      // The result contains a transaction hash that is every time different and should match the hash returned in the result
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");
      assert.equal(
        result.result,
        transactionHash,
        "has result:" + result.result + ", expected transaction hash:" + transactionHash
      );
    });
  });
});
