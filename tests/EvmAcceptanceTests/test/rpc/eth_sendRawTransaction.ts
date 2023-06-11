import {assert} from "chai";
import hre from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {web3} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_sendRawTransaction";

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", function () {
    it("should return a send raw transaction", async function () {
      const fromAccount = web3.eth.accounts.privateKeyToAccount(hre.network["config"]["accounts"][0]);
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
  });
});
