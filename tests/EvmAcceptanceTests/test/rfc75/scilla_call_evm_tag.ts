import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("RFC75 ScillaCallEvmTag", function () {
  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;

  const VAL = 10;

  beforeEach(async function () {
    solidityContract = await hre.deployContract("ScillaCall");

    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    scillaContract = await parallelizer.deployScillaContract("ScillaCallSimple");
    scillaContractAddress = scillaContract.address?.toLowerCase()!;
  });

  it("Should be deployed successfully", async function () {
    expect(solidityContract.address).to.be.properAddress;
    expect(scillaContract.address).to.be.properAddress;
  });

  it("Should be reverted when a message from scilla is sent with _EvmCall tag and with call_mode = 0", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScilla(scillaContractAddress, "callWithEvmTag", CALL_MODE, solidityContract.address, VAL)
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent with _EvmCall tag and with call_mode = 1", async function () {
    const CALL_MODE = 1;
    await expect(
      solidityContract.callScilla(scillaContractAddress, "callWithEvmTag", CALL_MODE, solidityContract.address, VAL)
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent with _EvmCall tag and with call_mode > 1", async function () {
    const CALL_MODE = 2;
    await expect(
      solidityContract.callScilla(scillaContractAddress, "callWithEvmTag", CALL_MODE, solidityContract.address, VAL)
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent with _EvmCall tag and with call_mode < 0", async function () {
    const CALL_MODE = -1;
    await expect(
      solidityContract.callScilla(scillaContractAddress, "callWithEvmTag", CALL_MODE, solidityContract.address, VAL)
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent with _EvmCall tag and with wrong transition name", async function () {
    const CALL_MODE = 1;
    await expect(
      solidityContract.callScillaInsufficientParams(
        scillaContractAddress,
        "callXYZ",
        CALL_MODE,
        solidityContract.address
      )
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent with _EvmCall tag and with call_mode = 0", async function () {
    const CALL_MODE = 0;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "callWithEvmTag",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent with _EvmCall tag and with call_mode = 1", async function () {
    const CALL_MODE = 1;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "callWithEvmTag",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent with _EvmCall tag and with call_mode < 0", async function () {
    const CALL_MODE = -1;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "callWithEvmTag",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent with _EvmCall tag and with call_mode > 1", async function () {
    const CALL_MODE = 2;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "callWithEvmTag",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent with _EvmCall tag and with wrong transition name", async function () {
    const CALL_MODE = 1;
    let resp = await solidityContract.callScillaInsufficientParams(
      scillaContractAddress,
      "callXYZ",
      CALL_MODE,
      solidityContract.address,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });
});
