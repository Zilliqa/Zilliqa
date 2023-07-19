import {assert, expect} from "chai";
import {ethers} from "hardhat";
import {expectRevert} from "@openzeppelin/test-helpers";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";
import {parallelizer} from "../helpers";

describe("Otterscan api tests", function () {

  before(async function () {
      const METHOD = "ots_enable";

      // Make sure traceing is enabled
      await sendJsonRpcRequest(METHOD, 1, [true], (result, status) => {
        assert.equal(status, 200, "has status code");
      });

  });

  it("When we revert the TX, we can get the tx error ", async function () {
    const METHOD = "ots_getTransactionError";
    const REVERT_MESSAGE = "Transaction too old";

    const abi = ethers.utils.defaultAbiCoder;
    const MESSAGE_ENCODED = "0x08c379a0" + abi.encode(["string"], [REVERT_MESSAGE]).split("x")[1];

    const Contract = await ethers.getContractFactory("Revert");
    this.contract = await Contract.deploy();

    // In order to make a tx that fails at runtime and not estimate gas time, we estimate the gas of
    // a similar passing call and use this (+30% leeway) to override the gas field
    const estimatedGas = await this.contract.estimateGas.requireCustom(true, REVERT_MESSAGE);

    const tx = await this.contract.requireCustom(false, REVERT_MESSAGE, {gasLimit: estimatedGas.mul(130).div(100)});

    await sendJsonRpcRequest(METHOD, 1, [tx.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject, MESSAGE_ENCODED);
    });
  });

  it("When TX succeeds, we can get 0x", async function () {
    const METHOD = "ots_getTransactionError";

    // Create a simple contract creation transaction that will succeed and so API request returns "Ox"
    const Contract = await ethers.getContractFactory("SimpleContract");
    const contract = await Contract.deploy();

    await sendJsonRpcRequest(METHOD, 1, [contract.deployTransaction.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject, "0x");
    });
  });

  it("We can get the otter internal operations", async function () {
    const METHOD = "ots_getInternalOperations";

    // Check we can for example detect a suicide with correct value.
    // Below is taken from transfer.ts test
    const ACCOUNTS_COUNT = 3;
    const ACCOUNT_VALUE = 123_000_000;

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const addresses = accounts.map((signer) => signer.address);

    const tx = await parallelizer.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNTS_COUNT * ACCOUNT_VALUE
    });

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));

    await sendJsonRpcRequest(METHOD, 1, [tx.deployTransaction.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert.equal(jsonObject[0]["type"], 0, "has correct type for transfer");
      assert.equal(jsonObject[3]["type"], 1, "has correct type for self destruct");
    });
  });

  it("When a contract has no internal operations, we get empty list", async function () {
    const METHOD = "ots_getInternalOperations";

    // Create a simple contract creation transaction which involves no internal operations
    // so API call returns []
    const Contract = await ethers.getContractFactory("SimpleContract");
    const contract = await Contract.deploy();

    await sendJsonRpcRequest(METHOD, 1, [contract.deployTransaction.hash], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;

      assert(Array.isArray(jsonObject) && !jsonObject.length);
    });
  });

  it("We can get the otter trace transaction", async function () {
    const METHOD = "ots_traceTransaction";

    let contractOne = await parallelizer.deployContract("ContractOne");
    let contractTwo = await parallelizer.deployContract("ContractTwo");
    let contractThree = await parallelizer.deployContract("ContractThree");

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

  it("We can get the otter search for sender by nonce", async function () {
    const METHOD = "ots_getTransactionBySenderAndNonce";

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

  it("We can get the otter search TX before and after", async function () {
    const METHOD_BEFORE = "ots_searchTransactionsBefore";
    const METHOD_AFTER = "ots_searchTransactionsAfter";

    // run the contract that batch sends funds to other addresses
    // then we can check that this txid comes up when asking about
    // these contract addresses.
    const ACCOUNTS_COUNT = 3;
    const ACCOUNT_VALUE = 123_000_000;

    // Get the block height so we can check before/after
    const height = await ethers.provider.getBlockNumber();

    const accounts = Array.from({length: ACCOUNTS_COUNT}, (v, k) =>
      ethers.Wallet.createRandom().connect(ethers.provider)
    );

    const addresses = accounts.map((signer) => signer.address);

    const tx = await parallelizer.deployContract("BatchTransferCtor", addresses, ACCOUNT_VALUE, {
      value: ACCOUNTS_COUNT * ACCOUNT_VALUE
    });

    const balances = await Promise.all(accounts.map((account) => account.getBalance()));
    balances.forEach((el) => expect(el).to.be.eq(ACCOUNT_VALUE));

    await sendJsonRpcRequest(METHOD_AFTER, 1, [addresses[0], height, 100], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.equal(jsonObject.txs[0].hash, tx.deployTransaction.hash, "Can find the TX which send funds to this addr");
    });

    // There should be nothing before this point
    await sendJsonRpcRequest(METHOD_BEFORE, 1, [addresses[0], height, 100], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.equal(jsonObject.txs.length, 0, "Can not find the TX which send funds to this addr");
    });
  });
});
