import {assert, expect} from "chai";
import hre, {ethers} from "hardhat";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {Contract, Wallet} from "ethers";

const METHOD_BEFORE = "ots_searchTransactionsBefore";
const METHOD_AFTER = "ots_searchTransactionsAfter";
describe(`Otterscan api tests: ${METHOD_AFTER} #parallel`, function () {
  const ACCOUNTS_COUNT = 3;
  const ACCOUNT_VALUE = 123_000_000;
  let height: number;
  let accounts: Wallet[];
  let addresses: string[];
  let contract: Contract;

  before(async function () {
    const METHOD_ENABLE = "ots_enable";

    // Make sure tracing is enabled
    await sendJsonRpcRequest(METHOD_ENABLE, 1, [true], (result, status) => {
      assert.equal(status, 200, "has status code");
    });

    // Get the block height so we can check before/after
    height = await ethers.provider.getBlockNumber();

    accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) => ethers.Wallet.createRandom().connect(ethers.provider));

    addresses = accounts.map((signer) => signer.address);

    contract = await hre.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNTS_COUNT * ACCOUNT_VALUE
    });
  });

  xit("We can get the otter search TX before and after @block-1", async function () {
    // run the contract that batch sends funds to other addresses
    // then we can check that this txid comes up when asking about
    // these contract addresses.

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));

    await sendJsonRpcRequest(METHOD_AFTER, 1, [addresses[0], height, 100], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.equal(
        jsonObject.txs[0].hash,
        contract.deployTransaction.hash,
        "Can find the TX which send funds to this addr"
      );
    });

    // There should be nothing before this point
    await sendJsonRpcRequest(METHOD_BEFORE, 1, [addresses[0], height, 100], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.equal(jsonObject.txs.length, 0, "Can not find the TX which send funds to this addr");
    });
  });
});
