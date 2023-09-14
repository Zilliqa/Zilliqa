import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("RFC75 ScillaCallEOA", function () {
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

  it("Should not be reverted when a message from scilla is sent to eoa account with call_mode = 0", async function () {
    const CALL_MODE = 0;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .not.to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });

  it("Should not be reverted when a message from scilla is sent to eoa account with call_mode = 1", async function () {
    const CALL_MODE = 1;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .not.to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });

  it("Should be reverted when a message from scilla is sent to eoa account with call_mode > 1", async function () {
    const CALL_MODE = 2;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to eoa account with call_mode < 0", async function () {
    const CALL_MODE = -1;
    await expect(solidityContract.callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL))
      .to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("Should be reverted when a message from scilla is sent to eoa account with wrong transition name", async function () {
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

  it("Should change state when a message from scilla is sent to eoa account with call_mode = 0", async function () {
    const CALL_MODE = 0;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).not.to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });

  it("Should change state when a message from scilla is sent to eoa account with call_mode = 1", async function () {
    const CALL_MODE = 1;
    let resp = await solidityContract.callScilla(
      scillaContractAddress,
      "call",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 1000000}
    );
    await expect(resp.wait()).not.to.be.rejected;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });

  it("Should not change state when a message from scilla is sent to eoa account with call_mode < 0", async function () {
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

  it("Should not change state when a message from scilla is sent to eoa account with call_mode > 1", async function () {
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

  it("Should not change state when a message from scilla is sent to eoa account with wrong transition name", async function () {
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

  // Disabled in q4-working-branch
  xit("Should deduct the same amount from account as advertised in receipt", async function () {
    const CALL_MODE = 0;
    const admin = await solidityContract.signer;
    const initialBalance = await admin.getBalance();
    let tx = await solidityContract
      .connect(admin)
      .callScilla(scillaContractAddress, "call", CALL_MODE, solidityContract.address, VAL);
    tx = await tx.wait();
    expect(await scillaContract.value()).to.be.eq(VAL);
    const gasCost = tx.effectiveGasPrice.mul(tx.gasUsed);
    const finalBalance = await admin.getBalance();
    expect(finalBalance).to.be.eq(initialBalance.sub(gasCost));
  });

  it("Should not revert when there is another scilla contract in the chain and last scilla contract sends msg back to evm", async function () {
    const CALL_MODE = 0;
    // Deploy intermediate scilla contract that is called by evm precompile and forwards the call to
    // dest scilla contract which eventually sends msg back to evm contract
    const interScillaContract = await parallelizer.deployScillaContract("ScillaCallSimple");
    const interScillaContractAddress = interScillaContract.address?.toLowerCase()!;

    await expect(
      solidityContract.callForwardScilla(
        interScillaContractAddress,
        "forward",
        CALL_MODE,
        scillaContractAddress,
        solidityContract.address,
        VAL
      )
    ).not.to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });
});
