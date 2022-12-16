const {expect} = require("chai");
const {ethers, web3} = require("hardhat");
const parallelizer = require("../../helper/Parallelizer");
const web3_helper = require("../../helper/Web3Helper");

const ENUM = 1;
const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
const UINT256 = 100;
const STRING = "ZILLIQA";

describe("Contract Interaction with web3.js", function () {
  describe("When public setter function is called", function () {
    let contract;
    const gasLimit = "750000";
    let options;
    before(async function () {
      contract = await web3_helper.deploy("WithSettersAndGetters", {gasLimit});
      options = await web3_helper.getCommonOptions();
    });

    it("Should set uint256 internal state correctly", async function () {
      const TO_BE_SET = 100;
      expect(await contract.methods.setNumber(TO_BE_SET).send(options)).to.be.not.null;
      expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(TO_BE_SET));
    });

    it("Should set uint256 internal state correctly with external method", async function () {
      const TO_BE_SET = 100;
      expect(await contract.methods.setNumberExternal(TO_BE_SET).send(options)).to.be.not.null;
      expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(TO_BE_SET));
    });

    it("Should set string internal state correctly", async function () {
      const TO_BE_SET = "Zilliqa";
      expect(await contract.methods.setName(TO_BE_SET).send(options)).to.be.not.null;
      expect(await contract.methods.name().call()).to.be.eq(TO_BE_SET);
    });

    it("Should set string internal state correctly with external method", async function () {
      const TO_BE_SET = "Zilliqa";
      expect(await contract.methods.setNameExternal(TO_BE_SET).send(options)).to.be.not.null;
      expect(await contract.methods.name().call()).to.be.eq(TO_BE_SET);
    });

    it("Should set enum internal state correctly", async function () {
      const ENUM = "1";
      expect(await contract.methods.setEnum(ENUM).send(options)).to.be.not.null;
      expect(await contract.methods.someEnum().call()).to.be.eq(ENUM);
    });

    it("Should set address internal state correctly", async function () {
      const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      expect(await contract.methods.setAddress(ADDRESS).send(options)).to.be.not.null;
      expect(await contract.methods.someAddress().call()).to.be.eq(ADDRESS);
    });

    it("Should have event in returned object when no arg is specified", async function () {
      const sendResult = await contract.methods.emitLogWithoutParam().send(options);
      expect(sendResult.events.logWithoutParam).to.be.not.null;
    });

    it("Should have event in returned object when arg type is uint256", async function () {
      const sendResult = await contract.methods.emitLogWithUint256Param().send(options);
      expect(sendResult.events.logWithUint256Param).to.be.not.null;
      expect(sendResult.events.logWithUint256Param.returnValues.value).to.be.eq("234");
    });
  });
});
