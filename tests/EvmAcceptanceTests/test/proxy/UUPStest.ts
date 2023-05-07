import {expect} from "chai";
import {Contract, Signer, Wallet} from "ethers";
import {parallelizer} from "../../helpers";

describe("When using UUPS proxy for interacting with contract", function () {
  let proxyContract: Contract;
  let signer: Wallet;
  const INIT_VALUE = 10;
  before(async function () {
    signer = await parallelizer.takeSigner();
    proxyContract = await parallelizer.deployProxyWithSigner(signer, "uups", "ImplementationV1", "initialize", [
      INIT_VALUE
    ]);
  });

  it("Should be deployed successfully", async function () {
    expect(proxyContract.address).to.be.properAddress;
    expect(await proxyContract.getSecretValue()).to.be.eq(INIT_VALUE);
  });

  it("Should allow to set and retrieve a value correctly", async function () {
    const SOME_NEW_VAL = 20;
    expect(await proxyContract.setSecretValue(SOME_NEW_VAL)).not.to.be.reverted;
    expect(await proxyContract.getSecretValue()).to.be.eq(SOME_NEW_VAL);
  });

  it("Should not be possible to upgrade contract with different signer", async function () {
    const newSigner = await parallelizer.takeSigner();
    await expect(parallelizer.upgradeProxyWithSigner(newSigner, proxyContract, "ImplementationV2")).to.be.rejected;
  });

  it("Should be able to upgrade contract with original signer", async function () {
    proxyContract = await parallelizer.upgradeProxyWithSigner(signer, proxyContract, "ImplementationV2");
    expect(proxyContract.address).to.be.properAddress;
  });

  it("Should be able set and read new plain field in upgraded implementation", async function () {
    const SOME_NEW_VAL = 30;
    expect(await proxyContract.setSecretValue(SOME_NEW_VAL)).not.to.be.reverted;
    expect(await proxyContract.getSecretValue()).to.be.eq(SOME_NEW_VAL);
    // Counter starts with 0 upon proxy upgrade
    expect(await proxyContract.incrementCounter()).not.to.be.reverted;
    expect(await proxyContract.getCounter()).to.be.eq(1);
  });

  it("Should be able set and read map in upgraded implementation", async function () {
    const SOME_VAL = 1234;
    expect(await proxyContract.setValueInMap(signer.getAddress(), SOME_VAL)).not.to.be.reverted;
    expect(await proxyContract.getValueFromMap(signer.getAddress())).to.be.eq(SOME_VAL);
  });

  it("Should not overwrite previously defined values in storage", async function () {
    const SOME_PLAIN_VAL = 1234;
    const SOME_KEY_VAL = 4321;
    expect(await proxyContract.setSecretValue(SOME_PLAIN_VAL)).not.to.be.reverted;
    expect(await proxyContract.getSecretValue()).to.be.eq(SOME_PLAIN_VAL);
    expect(await proxyContract.setValueInMap(signer.getAddress(), SOME_KEY_VAL)).not.to.be.reverted;
    expect(await proxyContract.getValueFromMap(signer.getAddress())).to.be.eq(SOME_KEY_VAL);

    expect(await proxyContract.getSecretValue()).to.be.eq(SOME_PLAIN_VAL);
    expect(await proxyContract.getValueFromMap(signer.getAddress())).to.be.eq(SOME_KEY_VAL);
    expect(await proxyContract.getCounter()).to.be.eq(1);
  });

  it("Should not be able to initialize the same proxy contract twice", async function () {
    await expect(proxyContract.initialize(123)).to.be.rejected;
  });

  it("Should not be possible to upgrade contract with different signer", async function () {
    await expect(proxyContract.initialize(123)).to.be.rejected;
  });
});
