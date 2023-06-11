import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Manual nonce", function () {
  let contract: ScillaContract;
  const VALUE = 12;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("SetGet");
  });

  it("Should be possible to set nonce manually", async function () {
    let result = await parallelizer.zilliqaSetup.zilliqa.blockchain.getBalance(parallelizer.zilliqaAccountAddress);
    const nextNonce = result.result.nonce + 1;
    await contract.set(VALUE, {nonce: nextNonce});
    expect(await contract.value()).to.be.eq(VALUE);
  });

  // FIXME: in https://zilliqa-jira.atlassian.net/browse/ZIL-5199
  xit("Should be possible to call multiple transitions with manual nonces", async function () {
    let result = await parallelizer.zilliqaSetup.zilliqa.blockchain.getBalance(parallelizer.zilliqaAccountAddress);

    const NONCE = result.result.nonce;

    let txPromises = [];
    for (let i = 1; i <= 10; ++i) {
      txPromises.push(contract.set(i, {nonce: NONCE + i}));
    }

    await Promise.all(txPromises);
    console.log(contract.error);
    expect(await contract.value()).to.be.eq(10);
  });
});
