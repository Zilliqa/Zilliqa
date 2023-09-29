import {assert, expect} from "chai";
import hre, {ethers} from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {Contract, Wallet} from "ethers";

const METHOD = "ots_getInternalOperations";
describe(`Otterscan api tests: ${METHOD} #parallel`, function () {
  let contract: Contract;
  const ACCOUNT_VALUE = 123_000_000;
  let accounts: Wallet[];
  let addresses: string[];
  before(async function () {
    const METHOD_ENABLE = "ots_enable";

    // Make sure tracing is enabled
    await sendJsonRpcRequest(METHOD_ENABLE, 1, [true], (result, status) => {
      assert.equal(status, 200, "has status code");
    });

    const ACCOUNTS_COUNT = 3;

    accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) => ethers.Wallet.createRandom().connect(ethers.provider));

    addresses = accounts.map((signer) => signer.address);

    contract = await hre.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNTS_COUNT * ACCOUNT_VALUE
    });
  });

  it("We can get the otter internal operations @block-1", async function () {
    // Check we can for example detect a suicide with correct value.
    // Below is taken from transfer.ts test

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));

    await sendJsonRpcRequest(METHOD, 1, [contract.deployTransaction.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject[0]["type"], 0, "has correct type for transfer");
      assert.equal(jsonObject[3]["type"], 1, "has correct type for self destruct");
    });
  });
});
