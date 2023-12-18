import {expect} from "chai";
import hre, {ethers} from "hardhat";
import {Contract, utils} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";
import {defaultAbiCoder, toUtf8Bytes} from "ethers/lib/utils";

describe("RFC75 ScillaCallComplex", function () {
  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;

  const VAL = 99;

  beforeEach(async function () {
    solidityContract = await hre.deployContract("ScillaCallComplex");

    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    scillaContract = await parallelizer.deployScillaContract("ScillaCallComplex");
    scillaContractAddress = scillaContract.address?.toLowerCase()!;
  });

  it("Should be deployed successfully", async function () {
    expect(solidityContract.address).to.be.properAddress;
    expect(scillaContract.address).to.be.properAddress;
  });

  it("It should see updates states withing single solidity function call", async function () {
    const CALL_MODE = 0;
    await expect(
      solidityContract.SetAndGet(scillaContractAddress, "setValue", CALL_MODE, solidityContract.address, VAL)
    ).not.to.be.reverted;
  });

  it("It should attach scilla errors to receipt if scilla execution failed", async function () {
    const CALL_MODE = 0;
    const tx = await solidityContract.callScilla(
      scillaContractAddress,
      "emitEventAndFail",
      CALL_MODE,
      solidityContract.address,
      VAL,
      {gasLimit: 500000}
    );
    const receipt = await ethers.provider.getTransactionReceipt(tx.hash);
    expect(receipt.logs).to.have.length(3);
    {
      expect(defaultAbiCoder.decode(["string"], receipt.logs[0].data)[0]).to.eq("CALL_CONTRACT_FAILED");
      const EXPECTED_TOPIC_0 = utils.keccak256(toUtf8Bytes("ScillaError(string)"));
      expect(receipt.logs[0].topics[0].toLowerCase()).to.eq(EXPECTED_TOPIC_0.toLowerCase());
    }
    {
      expect(defaultAbiCoder.decode(["string"], receipt.logs[1].data)[0]).to.eq(
        'Exception thrown: (Message [(_exception : (String "Yaicksss"))]), line: 36'
      );
      const EXPECTED_TOPIC_0 = utils.keccak256(toUtf8Bytes("ScillaException(string)"));
      expect(receipt.logs[1].topics[0].toLowerCase()).to.eq(EXPECTED_TOPIC_0.toLowerCase());
    }
    {
      expect(defaultAbiCoder.decode(["string"], receipt.logs[2].data)[0]).to.eq(
        "Raised from emitEventAndFail, line: 32"
      );
      const EXPECTED_TOPIC_0 = utils.keccak256(toUtf8Bytes("ScillaException(string)"));
      expect(receipt.logs[2].topics[0].toLowerCase()).to.eq(EXPECTED_TOPIC_0.toLowerCase());
    }
  });
});
