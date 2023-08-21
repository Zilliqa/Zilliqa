import {expect} from "chai";
import hre from "hardhat";
import {Contract, utils} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("RFC75 ScillaCallReceiver", function () {
  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;

  const VAL = 10;

  beforeEach(async function () {
    solidityContract = await hre.deployContract("ScillaCallReceiver");

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

  it("Should support scilla receiver interface", async function () {
    const scillaHandlerSignature = utils.id("handle_scilla_message(string,bytes)").slice(0, 10);
    expect(await solidityContract.supportsInterface(scillaHandlerSignature)).to.be.true;
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with call_mode = 0", async function () {
    const CALL_MODE = 0;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with call_mode = 1", async function () {
    const CALL_MODE = 1;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with call_mode > 1", async function () {
    const CALL_MODE = 2;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with call_mode < 0", async function () {
    const CALL_MODE = -1;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with insufficient params", async function () {
    const CALL_MODE = 1;
    await expect(
      solidityContract.callScillaInsufficientParams(scillaContractAddress, "call", CALL_MODE, solidityContract.address)
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to evm having scilla receiver with wrong transition name", async function () {
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

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with call_mode = 0", async function () {
    const CALL_MODE = 0;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with call_mode = 1", async function () {
    const CALL_MODE = 1;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with call_mode < 0", async function () {
    const CALL_MODE = -1;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with call_mode > 1", async function () {
    const CALL_MODE = 2;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with insufficient params", async function () {
    const CALL_MODE = 1;
    let resp = await solidityContract.callScillaInsufficientParams(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should not change state when a message from scilla is sent to evm having scilla receiver with wrong transition name", async function () {
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
