import {expect} from "chai";
import hre, {web3} from "hardhat";

describe("Contract Interaction with web3.js", function () {
  describe("When public setter function is called", function () {
    before(async function () {
      this.contract = await hre.deployContractWeb3("WithSettersAndGetters", {});
    });

    it("Should set uint256 internal state correctly", async function () {
      const TO_BE_SET = 100;
      expect(await this.contract.methods.setNumber(TO_BE_SET).send()).to.be.not.null;
      expect(await this.contract.methods.number().call()).to.be.eq(web3.utils.toBN(TO_BE_SET));
    });

    it("Should set uint256 internal state correctly with external method", async function () {
      const TO_BE_SET = 100;
      expect(await this.contract.methods.setNumberExternal(TO_BE_SET).send()).to.be.not.null;
      expect(await this.contract.methods.number().call()).to.be.eq(web3.utils.toBN(TO_BE_SET));
    });

    it("Should set string internal state correctly", async function () {
      const TO_BE_SET = "Zilliqa";
      expect(await this.contract.methods.setName(TO_BE_SET).send()).to.be.not.null;
      expect(await this.contract.methods.name().call()).to.be.eq(TO_BE_SET);
    });

    it("Should set string internal state correctly with external method", async function () {
      const TO_BE_SET = "Zilliqa";
      expect(await this.contract.methods.setNameExternal(TO_BE_SET).send()).to.be.not.null;
      expect(await this.contract.methods.name().call()).to.be.eq(TO_BE_SET);
    });

    it("Should set enum internal state correctly", async function () {
      const ENUM = "1";
      expect(await this.contract.methods.setEnum(ENUM).send()).to.be.not.null;
      expect(await this.contract.methods.someEnum().call()).to.be.eq(ENUM);
    });

    it("Should set address internal state correctly", async function () {
      const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      expect(await this.contract.methods.setAddress(ADDRESS).send()).to.be.not.null;
      expect(await this.contract.methods.someAddress().call()).to.be.eq(ADDRESS);
    });

    it("Should have event in returned object when no arg is specified", async function () {
      const sendResult = await this.contract.methods.emitLogWithoutParam().send();
      expect(sendResult.events.logWithoutParam).to.be.not.null;
    });

    it("Should have event in returned object when arg type is uint256", async function () {
      const sendResult = await this.contract.methods.emitLogWithUint256Param().send();
      expect(sendResult.events.logWithUint256Param).to.be.not.null;
      expect(sendResult.events.logWithUint256Param.returnValues.value).to.be.eq("234");
    });
  });
});
