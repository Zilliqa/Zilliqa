const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("Openzeppelin address contract functionality", function () {
  before(async function () {
    const Contract = await ethers.getContractFactory("AddressImpl");
    this.contract = await Contract.deploy();
    await this.contract.deployed();
  });

  it("calls the requested function with transaction funds", async function () {
    const Contract = await ethers.getContractFactory("CallReceiverMock");
    contractRecipient = await Contract.deploy();
    await contractRecipient.deployed();
    const amount = ethers.utils.parseEther("1.2");

    const abiEncodedCall = web3.eth.abi.encodeFunctionCall({name: "mockFunction", type: "function", inputs: []}, []);

    await expect(
      this.contract.functionCallWithValue(contractRecipient.address, abiEncodedCall, amount, {value: amount})
    )
      .to.emit(this.contract, "CallReturnValue")
      .withArgs("0x1234")
      .to.emit(contractRecipient, "MockFunctionCalled");

    expect(await ethers.provider.getBalance(contractRecipient.address)).to.be.eq(amount);
  });
});
