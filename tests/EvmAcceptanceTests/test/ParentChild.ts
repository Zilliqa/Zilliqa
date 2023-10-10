import {assert, expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";
import logDebug from "../helpers/DebugHelper";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";

describe("Parent Child Contract Functionality #parallel", function () {
  const INITIAL_FUND = 10_000_000;
  const CHILD_CONTRACT_VALUE = 12345;
  let installedChild: Contract;
  let childContractAddress: string;
  let childContract: Contract;
  let parentContract: Contract;

  before(async function () {
    parentContract = await hre.deployContract("ParentContract", {value: INITIAL_FUND});
  });

  it(`Should return ${INITIAL_FUND} when getPaidValue is called @block-1`, async function () {
    expect(await parentContract.getPaidValue()).to.be.equal(INITIAL_FUND);
  });

  it(`Should return ${INITIAL_FUND} as the balance of the parent contract @block-1`, async function () {
    expect(await ethers.provider.getBalance(parentContract.address)).to.be.eq(INITIAL_FUND);
  });

  it("Should instantiate a new child if installChild is called @block-2", async function () {
    installedChild = await parentContract.installChild(CHILD_CONTRACT_VALUE, {gasLimit: 25000000});
    childContractAddress = await parentContract.childAddress();
    expect(childContractAddress).to.be.properAddress;
  });

  it(`Should return ${INITIAL_FUND} as the balance of the child contract @block-3`, async function () {
    expect(await ethers.provider.getBalance(childContractAddress)).to.be.eq(INITIAL_FUND);
  });

  it(`Should return ${CHILD_CONTRACT_VALUE} when read function of the child is called @block-3`, async function () {
    childContract = await hre.ethers.getContractAt("ChildContract", childContractAddress);
    childContract = childContract.connect(parentContract.signer);
    expect(await childContract.read()).to.be.eq(CHILD_CONTRACT_VALUE);
  });

  it("Should return parent address if sender function of child is called", async function () {
    expect(await childContract.sender()).to.be.eq(parentContract.address);
  });

  it("Should return all funds from the child to its sender contract if returnToSender is called", async function () {
    await childContract.returnToSender();
    expect(await ethers.provider.getBalance(parentContract.address)).to.be.eq(INITIAL_FUND);
    expect(await ethers.provider.getBalance(childContract.address)).to.be.eq(0);
  });
});
