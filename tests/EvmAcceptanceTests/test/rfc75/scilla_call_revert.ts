import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("RFC75 ScillaCallRevert", function () {
  let solidityContract: Contract;
  let solidityContractReceiver: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;

  const VAL = 99;

  beforeEach(async function () {
    solidityContract = await hre.deployContract("ScillaCallRevert");
    solidityContractReceiver = await hre.deployContract("ScillaCallReceiver");

    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    scillaContract = await parallelizer.deployScillaContract("ScillaCallRevert");
    scillaContractAddress = scillaContract.address?.toLowerCase()!;
  });

  it("Should be deployed successfully", async function () {
    expect(solidityContract.address).to.be.properAddress;
    expect(solidityContractReceiver.address).to.be.properAddress;
    expect(scillaContract.address).to.be.properAddress;
  });

  it("It should be reverted when one of sent messages from scilla contract is sent to evm contract implementing scila receiver", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScilla(
        scillaContractAddress,
        "callWithTwoRecipients",
        CALL_MODE,
        VAL,
        solidityContract.address,
        solidityContractReceiver.address
      )
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("It should be reverted when one of sent messages from scilla contract has _EvmCall tag", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScilla(
        scillaContractAddress,
        "callWithTwoRecipientsEvmCall",
        CALL_MODE,
        VAL,
        solidityContract.address,
        solidityContract.address
      )
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("It should not be reverted when messages sent from scilla contract are sent to other scilla contracts", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScilla(
        scillaContractAddress,
        "callWithTwoRecipients",
        CALL_MODE,
        VAL,
        scillaContractAddress,
        scillaContractAddress
      )
    ).not.to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(VAL);
  });

  it("It should be reverted when message sent in chain eventually triggers a message sent to evm contract with scillaReceiver", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScillaChain(
        scillaContractAddress,
        "callAndForward",
        CALL_MODE,
        VAL,
        scillaContractAddress,
        solidityContractReceiver.address,
        solidityContract.address
      )
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("It should be reverted when message sent in chain eventually triggers a message sent to evm contract with EvmCall tag", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScillaChain(
        scillaContractAddress,
        "callAndForwardToEvmCall",
        CALL_MODE,
        VAL,
        scillaContractAddress,
        solidityContract.address,
        solidityContract.address
      )
    ).to.be.reverted;
    expect(await scillaContract.value()).to.be.eq(0);
  });

  it("It should be reverted call to scilla is not successful", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.callScilla(
        scillaContractAddress,
        "justRevert",
        CALL_MODE,
        VAL,
        solidityContract.address,
        solidityContract.address
      )
    ).to.be.reverted;
  });
});
