const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");
const general_helper = require("../helper/GeneralHelper");

describe("Contract destruction with web3.js", function () {
  let contract;
  const gasLimit = "750000";
  let amountPaid;
  let options;
  before(function () {
    amountPaid = web3.utils.toBN(web3.utils.toWei("300", "gwei"));
  });

  describe("When a user method call", function () {
    before(async function () {
      contract = await web3_helper.deploy("ParentContract", {gasLimit, value: amountPaid});
      options = await web3_helper.getCommonOptions();
    });

    it("should be destructed and coins in the contract should be transferred to the address specified in the method [@transactional]", async function () {
      expect(await contract.methods.getPaidValue().call(options)).to.be.eq(amountPaid);
      const destAccount = await web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(1)).address;
      const prevBalance = await web3.eth.getBalance(destAccount);
      expect(
        await contract.methods
          .returnToSenderAndDestruct(destAccount)
          .send({gasLimit: 1000000, from: web3_helper.getPrimaryAccountAddress()})
      ).to.be.not.null;
      const newBalance = await web3.eth.getBalance(destAccount);
      // Dest Account should have prevBalance + amountPaid
      expect(web3.utils.toBN(newBalance)).to.be.equal(web3.utils.toBN(prevBalance).add(amountPaid));
    });
  });

  describe("When a method call happens through another contract", function () {
    before(async function () {
      contract = await web3_helper.deploy("ParentContract", {gasLimit, value: amountPaid});
      options = await web3_helper.getCommonOptions();
    });

    it("Should be destructed and coins in the contract should be transferred to the address specified in the method [@transactional]", async function () {
      const result = await contract.methods
        .installChild(123)
        .send({gasLimit: 1000000, from: web3_helper.getPrimaryAccountAddress()});
      expect(result).to.be.not.null;

      const childAddress = await contract.methods.childAddress().call(options);
      const childContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("ChildContract").abi, childAddress);

      const prevBalance = await web3.eth.getBalance(contract.options.address);
      await childContract.methods.returnToSender().send(options);
      const newBalance = await web3.eth.getBalance(contract.options.address);
      // Parent contract should have prevBalance + amountPaid
      expect(web3.utils.toBN(newBalance)).to.be.equal(web3.utils.toBN(prevBalance).add(amountPaid));
    });
  });
});

describe("Contract destruction with ethers.js", function () {
  describe("via user method call", function () {
    // TODO: Consider adding the test case when the receiving address does not exist.
    it("should be destructed and coins in the contract should be transferred to the address specified in the method");
  });
  describe("through a method call by another contract", function () {
    // TODO: Consider adding the test case when the receiving address does not exist.
    it("should be destructed and coins in the contract should be transferred to the address specified in the method");
  });
});
