import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";

describe("Delegatecall functionality #parallel", function () {
  let delegateContract: Contract;
  let testDelegateContract: Contract;
  let signer: SignerWithAddress;
  before(async function () {
    signer = hre.allocateEthSigner();
    delegateContract = await hre.deployContractWithSigner("Delegatecall", signer);
    testDelegateContract = await hre.deployContractWithSigner("TestDelegatecall", signer);
  });

  after(function () {
    hre.releaseEthSigner(signer);
  });

  it("should delegate function call correctly @block-1", async function () {
    const VALUE = 1000000;
    const NUM = 3735931646; // 0xDEADCAFE

    const owner = signer;
    await delegateContract.setVars(testDelegateContract.address, NUM, {value: VALUE});

    expect(await delegateContract.num()).to.be.eq(NUM);
    expect(await delegateContract.value()).to.be.eq(VALUE);
    expect(await delegateContract.sender()).to.be.eq(owner.address);
    expect(await ethers.provider.getBalance(delegateContract.address)).to.be.eq(VALUE);

    expect(await testDelegateContract.num()).to.be.eq(0);
    expect(await testDelegateContract.value()).to.be.eq(0);
    expect(await testDelegateContract.sender()).to.be.eq("0x0000000000000000000000000000000000000000");
    expect(await ethers.provider.getBalance(testDelegateContract.address)).to.be.eq(0);
  });
});
