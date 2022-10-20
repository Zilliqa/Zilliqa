const {expect} = require("chai");
const {ethers, web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");

const ENUM = 1;
const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
const UINT256 = 100;
const STRING = "ZILLIQA";

describe("Contract Interaction", function () {
  describe("When ethers.js is used", function () {
    let contract;
    before(async function () {
      const Contract = await ethers.getContractFactory("WithSettersAndGetters");
      contract = await Contract.deploy();
    });

    describe("When public setter function is called [@transactional]", function () {
      it("Should set uint256 internal state correctly", async function () {
        await contract.setNumber(UINT256);
        expect(await contract.number()).to.be.eq(UINT256);
      });

      it("Should set string internal state correctly [@transactional]", async function () {
        await contract.setName(STRING);
        expect(await contract.name()).to.be.eq(STRING);
      });

      it("Should set enum internal state correctly [@transactional]", async function () {
        await contract.setEnum(ENUM);
        expect(await contract.someEnum()).to.be.eq(ENUM);
      });

      it("Should set address internal state correctly [@transactional]", async function () {
        await contract.setAddress(ADDRESS);
        expect(await contract.someAddress()).to.be.eq(ADDRESS);
      });
    });

    describe("When external setter function is called", function () {
      it("Should set uint256 internal state correctly [@transactional]", async function () {
        await contract.setNumberExternal(UINT256);
        expect(await contract.number()).to.be.eq(UINT256);
      });

      it("Should set string internal state correctly [@transactional]", async function () {
        await contract.setNameExternal(STRING);
        expect(await contract.name()).to.be.eq(STRING);
      });

      it("Should set enum internal state correctly [@transactional]", async function () {
        await contract.setEnumExternal(ENUM);
        expect(await contract.someEnum()).to.be.eq(ENUM);
      });

      it("Should set address internal state correctly [@transactional]", async function () {
        await contract.setAddressExternal(ADDRESS);
        expect(await contract.someAddress()).to.be.eq(ADDRESS);
      });
    });

    describe("When public view function is called", function () {
      it("Should return correct value for uint256 [@transactional]", async function () {
        await contract.setNumber(UINT256);
        expect(await contract.getNumberPublic()).to.be.eq(UINT256);
      });

      it("Should return correct value for string [@transactional]", async function () {
        await contract.setName(STRING);
        expect(await contract.getStringPublic()).to.be.eq(STRING);
      });

      it("Should return correct value for address [@transactional]", async function () {
        await contract.setAddress(ADDRESS);
        expect(await contract.getAddressPublic()).to.be.eq(ADDRESS);
      });

      it("Should return correct value for enum [@transactional]", async function () {
        await contract.setEnum(ENUM);
        expect(await contract.getEnumPublic()).to.be.eq(ENUM);
      });
    });

    describe("When external view function is called", function () {
      it("Should return correct value for uint256", async function () {
        expect(await contract.getNumberExternal()).to.be.eq(UINT256);
      });

      it("Should return correct value for string", async function () {
        expect(await contract.getNameExternal()).to.be.eq(STRING);
      });

      it("Should return correct value for address", async function () {
        expect(await contract.getAddressExternal()).to.be.eq(ADDRESS);
      });

      it("Should return correct value for enum", async function () {
        expect(await contract.getEnumExternal()).to.be.eq(ENUM);
      });
    });

    describe("When public pure function is called", function () {
      it("Should return correct value for uint256", async function () {
        expect(await contract.getNumberPure()).to.be.eq(1);
      });

      it("Should return correct value for string", async function () {
        expect(await contract.getStringPure()).to.be.eq("Zilliqa");
      });

      it("Should return correct value for enum", async function () {
        expect(await contract.getEnumPure()).to.be.eq(2);
      });

      it("Should return correct value for tuple", async function () {
        expect(await contract.getTuplePure()).to.deep.equal([ethers.BigNumber.from(123), "zilliqa"]);
      });

      // TODO: Add for address.
    });

    describe("When external pure function is called", function () {
      it("Should return correct value for uint256", async function () {
        expect(await contract.getNumberPureExternal()).to.be.eq(1);
      });

      it("Should return correct value for string", async function () {
        expect(await contract.getStringPureExternal()).to.be.eq("Zilliqa");
      });

      it("Should return correct value for enum", async function () {
        expect(await contract.getEnumPureExternal()).to.be.eq(2);
      });

      it("Should return correct value for tuple", async function () {
        expect(await contract.getTuplePureExternal()).to.deep.equal([ethers.BigNumber.from(123), "zilliqa"]);
      });

      // TODO: Add for address
    });

    describe("When calling a public function that generates an event", function () {
      it("Should emit an event if emitLogWithoutParam is called", async function () {
        await expect(contract.emitLogWithoutParam()).emit(contract, "logWithoutParam");
      });

      it("Should emit an event with uint256 param if emitLogWithUint256Param is called", async function () {
        await expect(contract.emitLogWithUint256Param()).emit(contract, "logWithUint256Param").withArgs(234);
      });

      it("Should emit an event with string param if emitLogWithStringParam is called", async function () {
        await expect(contract.emitLogWithStringParam()).emit(contract, "logWithStringParam").withArgs("zilliqa");
      });

      it("Should emit an event with enum param if emitLogWithEnumParam is called", async function () {
        await expect(contract.emitLogWithEnumParam()).emit(contract, "logWithEnumParam").withArgs(0);
      });

      it("Should emit an event with address param if emitLogWithAddressParam is called", async function () {
        await expect(contract.emitLogWithAddressParam())
          .emit(contract, "logWithAddressParam")
          .withArgs(contract.address);
      });

      it("Should have the event arguments in returned object when event has multiple args", async function () {
        await expect(contract.emitLogWithMultiParams()).emit(contract, "logWithMultiParams").withArgs(123, "zilliqa");
      });

      // TODO: Calling a method on a contract that generates anonymous events with indexed parameters
      // TODO: Calling a method on a contract that generates non-anonymous events with indexed parameters
    });
  });

  describe("When web3.js is used", function () {
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
});
