import {assert, expect} from "chai";
import hre, {ethers} from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";

const METHOD = "ots_getTransactionBySenderAndNonce";
describe(`Otterscan api tests: ${METHOD}`, function () {
  before(async function () {
    const METHOD_ENABLE = "ots_enable";

    // Make sure tracing is enabled
    await sendJsonRpcRequest(METHOD_ENABLE, 1, [true], (result, status) => {
      assert.equal(status, 200, "has status code");
    });
  });

  it("We can get the otter search for sender by nonce", async function () {
    // To test this, send money to an account then have it send it back.
    // The nonces should be able to lookup via 0, 1, 2
    // re-use the batch transfer code for this
    const ACCOUNTS_COUNT = 1;
    const ACCOUNT_VALUE = 100_000_000;

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const acctAddr = accounts[0].address;

    const [owner] = await ethers.getSigners();
    let txRawFromOwner = {
      to: acctAddr,
      value: ethers.utils.parseEther("1")
    };
    await owner.sendTransaction(txRawFromOwner);

    // Create a transaction object
    let txRaw = {
      to: owner.address,
      value: ethers.utils.parseEther("0.45")
    };
    const txid0 = await accounts[0].sendTransaction(txRaw);
    const txid1 = await accounts[0].sendTransaction(txRaw);

    await sendJsonRpcRequest(METHOD, 1, [acctAddr, 0], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject, txid0.hash, "has correct hash");
    });

    await sendJsonRpcRequest(METHOD, 1, [acctAddr, 1], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject, txid1.hash, "has correct hash");
    });
  });
});
