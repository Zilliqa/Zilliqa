import {expect} from "chai";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {Account, Zilliqa} from "@zilliqa-js/zilliqa";

describe("Scilla HelloWorld contract #parallel", function () {
  let contract: ScillaContract;
  let signer: Account;
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    signer = hre.allocateZilSigner();
    contract = await hre.deployScillaContractWithSigner("HelloWorld", signer, signer.address);
  });

  after(function () {
    hre.releaseZilSigner(signer);
  });

  it("Should be deployed successfully @block-1", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Can be possible to call setHello by the owner @block-1", async function () {
    const tx = await contract.setHello("salam");
    expect(tx).to.eventLogWithParams("setHello()", {value: "2", vname: "code"});
    expect(await contract.welcome_msg()).to.be.eq("salam");
  });

  it("Should send getHello() event when getHello() transition is called @block-2", async function () {
    const tx = await contract.getHello();
    expect(tx).to.have.eventLogWithParams("getHello()", {value: "salam", vname: "msg", type: "String"});
  });

  it("Should cost gas for failed transaction due to execution error", async function () {
    const zilliqa = new Zilliqa(hre.getNetworkUrl());
    const balanceBefore = await zilliqa.blockchain.getBalance(signer.address);
    await contract.throwError();
    const balanceAfter = await zilliqa.blockchain.getBalance(signer.address);
    expect(Number.parseInt(balanceAfter.result.balance)).to.be.lt(Number.parseInt(balanceBefore.result.balance));
  });

  it("Should cost gas for failed transaction due to too low gas", async function () {
    const zilliqa = new Zilliqa(hre.getNetworkUrl());
    const balanceBefore = await zilliqa.blockchain.getBalance(signer.address);
    await contract.getHello({gasLimit: 300});
    const balanceAfter = await zilliqa.blockchain.getBalance(signer.address);
    expect(Number.parseInt(balanceAfter.result.balance)).to.be.lt(Number.parseInt(balanceBefore.result.balance));
  });
});
