import {expect} from "chai";
import {Contract} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";

describe("InteropErrors X045", function () {
  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;
  const KEEP_ORIGIN = 0;
  const IMMUTABLE_UINT = 12344321;
  const IMMUTABLE_INT = -12345;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    solidityContract = await hre.deployContract("InteropErrors");
    scillaContract = await parallelizer.deployScillaContract(
      "Thrower");
  });

  it("Should get an error when we call Scilla improperly through EVM", async function () {
    await solidityContract.callString(scillaContractAddress, "throwError", KEEP_ORIGIN, "Fish");
  });
});
