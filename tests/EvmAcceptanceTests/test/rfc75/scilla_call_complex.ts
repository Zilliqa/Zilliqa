import {expect} from "chai";
import hre from "hardhat";
import {Contract} from "ethers";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

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
});
