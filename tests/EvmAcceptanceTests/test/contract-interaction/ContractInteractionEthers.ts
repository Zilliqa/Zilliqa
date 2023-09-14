import {expect} from "chai";
import hre, {ethers} from "hardhat";

const ENUM = 1;
const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
const UINT256 = 100;
const STRING = "ZILLIQA";

describe("Contract Interaction with Ethers.js", function () {
  before(async function () {
    this.contract = await hre.deployContract("WithSettersAndGetters");
  });

  describe("When public setter function is called [@transactional]", function () {
    it("Should set uint256 internal state correctly", async function () {
      await this.contract.setNumber(UINT256);
      expect(await this.contract.number()).to.be.eq(UINT256);
    });

    it("Should set string internal state correctly [@transactional]", async function () {
      await this.contract.setName(STRING);
      expect(await this.contract.name()).to.be.eq(STRING);
    });

    it("Should set enum internal state correctly [@transactional]", async function () {
      await this.contract.setEnum(ENUM);
      expect(await this.contract.someEnum()).to.be.eq(ENUM);
    });

    it("Should set address internal state correctly [@transactional]", async function () {
      await this.contract.setAddress(ADDRESS);
      expect(await this.contract.someAddress()).to.be.eq(ADDRESS);
    });
  });

  describe("When external setter function is called", function () {
    it("Should set uint256 internal state correctly [@transactional]", async function () {
      await this.contract.setNumberExternal(UINT256);
      expect(await this.contract.number()).to.be.eq(UINT256);
    });

    it("Should set string internal state correctly [@transactional]", async function () {
      await this.contract.setNameExternal(STRING);
      expect(await this.contract.name()).to.be.eq(STRING);
    });

    it("Should set enum internal state correctly [@transactional]", async function () {
      await this.contract.setEnumExternal(ENUM);
      expect(await this.contract.someEnum()).to.be.eq(ENUM);
    });

    it("Should set address internal state correctly [@transactional]", async function () {
      await this.contract.setAddressExternal(ADDRESS);
      expect(await this.contract.someAddress()).to.be.eq(ADDRESS);
    });
  });

  describe("When public view function is called", function () {
    it("Should return correct value for uint256 [@transactional]", async function () {
      await this.contract.setNumber(UINT256);
      expect(await this.contract.getNumberPublic()).to.be.eq(UINT256);
    });

    it("Should return correct value for string [@transactional]", async function () {
      await this.contract.setName(STRING);
      expect(await this.contract.getStringPublic()).to.be.eq(STRING);
    });

    it("Should return correct value for address [@transactional]", async function () {
      await this.contract.setAddress(ADDRESS);
      expect(await this.contract.getAddressPublic()).to.be.eq(ADDRESS);
    });

    it("Should return correct value for enum [@transactional]", async function () {
      await this.contract.setEnum(ENUM);
      expect(await this.contract.getEnumPublic()).to.be.eq(ENUM);
    });
  });

  describe("When external view function is called", function () {
    it("Should return correct value for uint256", async function () {
      expect(await this.contract.getNumberExternal()).to.be.eq(UINT256);
    });

    it("Should return correct value for string", async function () {
      expect(await this.contract.getNameExternal()).to.be.eq(STRING);
    });

    it("Should return correct value for address", async function () {
      expect(await this.contract.getAddressExternal()).to.be.eq(ADDRESS);
    });

    it("Should return correct value for enum", async function () {
      expect(await this.contract.getEnumExternal()).to.be.eq(ENUM);
    });
  });

  describe("When public pure function is called", function () {
    it("Should return correct value for uint256", async function () {
      expect(await this.contract.getNumberPure()).to.be.eq(1);
    });

    it("Should return correct value for string", async function () {
      expect(await this.contract.getStringPure()).to.be.eq("Zilliqa");
    });

    it("Should return correct value for enum", async function () {
      expect(await this.contract.getEnumPure()).to.be.eq(2);
    });

    it("Should return correct value for tuple", async function () {
      expect(await this.contract.getTuplePure()).to.deep.equal([ethers.BigNumber.from(123), "zilliqa"]);
    });

    // TODO: Add for address.
  });

  describe("When external pure function is called", function () {
    it("Should return correct value for uint256", async function () {
      expect(await this.contract.getNumberPureExternal()).to.be.eq(1);
    });

    it("Should return correct value for string", async function () {
      expect(await this.contract.getStringPureExternal()).to.be.eq("Zilliqa");
    });

    it("Should return correct value for enum", async function () {
      expect(await this.contract.getEnumPureExternal()).to.be.eq(2);
    });

    it("Should return correct value for tuple", async function () {
      expect(await this.contract.getTuplePureExternal()).to.deep.equal([ethers.BigNumber.from(123), "zilliqa"]);
    });

    // TODO: Add for address
  });

  describe("When calling a public function that generates an event", function () {
    it("Should emit an event if emitLogWithoutParam is called", async function () {
      await expect(this.contract.emitLogWithoutParam()).emit(this.contract, "logWithoutParam");
    });

    it("Should emit an event with uint256 param if emitLogWithUint256Param is called", async function () {
      await expect(this.contract.emitLogWithUint256Param()).emit(this.contract, "logWithUint256Param").withArgs(234);
    });

    it("Should emit an event with string param if emitLogWithStringParam is called", async function () {
      await expect(this.contract.emitLogWithStringParam())
        .emit(this.contract, "logWithStringParam")
        .withArgs("zilliqa");
    });

    it("Should emit an event with enum param if emitLogWithEnumParam is called", async function () {
      await expect(this.contract.emitLogWithEnumParam()).emit(this.contract, "logWithEnumParam").withArgs(0);
    });

    it("Should emit an event with address param if emitLogWithAddressParam is called", async function () {
      await expect(this.contract.emitLogWithAddressParam())
        .emit(this.contract, "logWithAddressParam")
        .withArgs(this.contract.address);
    });

    it("Should have the event arguments in returned object when event has multiple args", async function () {
      await expect(this.contract.emitLogWithMultiParams())
        .emit(this.contract, "logWithMultiParams")
        .withArgs(123, "zilliqa");
    });

    // TODO: Calling a method on a contract that generates anonymous events with indexed parameters
    // TODO: Calling a method on a contract that generates non-anonymous events with indexed parameters
  });
});
