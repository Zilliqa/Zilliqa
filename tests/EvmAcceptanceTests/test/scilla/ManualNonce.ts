import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {Account} from "@zilliqa-js/zilliqa";

describe("Manual nonce #parallel", function () {
  let contract: ScillaContract;
  let signer: Account;
  const VALUE = 12;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    signer = hre.allocateZilSigner();
    contract = await hre.deployScillaContractWithSigner("SetGet", signer);
  });

  it("Should be possible to set nonce manually @block-1", async function () {
    let result = await hre.zilliqaSetup.zilliqa.blockchain.getBalance(signer.address);
    const nextNonce = result.result.nonce + 1;
    await contract.set(VALUE, {nonce: nextNonce});
    expect(await contract.value()).to.be.eq(VALUE);
  });

  it("Should be possible to call multiple transitions with manual nonces @block-2", async function () {
    if (hre.getNetworkName() === "isolated_server") {
      // This test doesn't work on iso server, but does on a devnet
      this.skip();
    }
  
    let result = await hre.zilliqaSetup.zilliqa.blockchain.getBalance(signer.address);

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
