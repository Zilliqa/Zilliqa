const gHelper = require("../../helper/GeneralHelper");
const zilliqa_helper = require("../../helper/ZilliqaHelper");
assert = require("chai").assert;

const METHOD = "eth_sendRawTransaction";

let amount = 1_000;

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", function () {
    it("should return a send raw transaction", async function () {
      const fromAccount = zilliqa_helper.primaryAccount;
      const toAddress = zilliqa_helper.getSecondaryAccountAddress();
      const nonce = await web3.eth.getTransactionCount(fromAccount.address); // nonce starts counting from 0
      const tx = {
        to: toAddress,
        value: amount,
        gas: 25000,
        gasPrice: 2000000000,
        nonce: nonce,
        chainId: gHelper.getEthChainId(),
        data: ""
      };

      const signedTx = await fromAccount.signTransaction(tx);

      await gHelper.callEthMethod(METHOD, 1, [signedTx.rawTransaction], (result, status) => {
        hre.logDebug("Result:", result);

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
