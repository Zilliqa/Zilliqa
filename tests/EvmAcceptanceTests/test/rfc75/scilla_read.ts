import {expect} from "chai";
import {Contract} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("RFC75 ScillaRead", function () {
  // Keys used in all tests cases
  const addr1 = "0xB3F90B06a7Dd9a860f8722f99B17fAce5abcb259";
  const addr2 = "0xc8532d4c6354D717163fAa8B7504b2b4436D20d1";
  const KEEP_ORIGIN = 0;
  const IMMUTABLE_UINT = 12344321;
  const IMMUTABLE_INT = -12345;
  const IMMUTABLE_STRING = "Salam"; // Means hello in Persian :)

  let solidityContract: Contract;
  let scillaContract: ScillaContract;
  let scillaContractAddress: string;

  before(async function () {
    solidityContract = await hre.deployContract("ScillaRead");

    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    scillaContract = await parallelizer.deployScillaContract(
      "ScillaRead",
      IMMUTABLE_UINT,
      IMMUTABLE_INT,
      IMMUTABLE_STRING,
      addr1
    );
    scillaContractAddress = scillaContract.address?.toLowerCase()!;
  });

  it("Should be deployed successfully", async function () {
    expect(solidityContract.address).to.be.properAddress;
    expect(scillaContract.address).to.be.properAddress;
  });

  describe("When call is performed from solidity to scilla contract", function () {
    it("It should return proper string after invoking set method with string arg", async function () {
      const someString = "SomeString";
      await solidityContract.callString(scillaContractAddress, "setString", KEEP_ORIGIN, someString);
      let readString = await solidityContract.readString(scillaContractAddress, "strField");
      expect(readString).to.be.equal(someString);
    });

    it("It should return proper integer after invoking set method for simpleMap", async function () {
      const VAL = 1000;
      await solidityContract.callSimpleMap(scillaContractAddress, "setSimpleMap", KEEP_ORIGIN, addr1, VAL);
      let readRes = await solidityContract.readSimpleMap(scillaContractAddress, "simpleMap", addr1);
      expect(readRes).to.be.eq(VAL);
    });

    it("It should return proper integer after invoking set method for nestedMap", async function () {
      const VAL = 2000;
      await solidityContract.callNestedMap(scillaContractAddress, "setNestedMap", KEEP_ORIGIN, addr1, addr2, VAL);
      let readRes = await solidityContract.readNestedMap(scillaContractAddress, "nestedMap", addr1, addr2);
      expect(readRes.toNumber()).to.be.eq(VAL);
    });

    it("It should return proper unsigned integer after invoking set method with integer arg", async function () {
      const NUM = 12345;
      await solidityContract.callUint(scillaContractAddress, "setUint", KEEP_ORIGIN, NUM);
      let readRes = await solidityContract.readUint(scillaContractAddress, "uintField");
      expect(readRes).to.be.eq(NUM);
    });

    it("It should return proper integer after invoking set method with integer arg", async function () {
      const NUM = -12345;
      await solidityContract.callInt(scillaContractAddress, "setInt", KEEP_ORIGIN, NUM);
      let readRes = await solidityContract.readInt(scillaContractAddress, "intField");
      expect(readRes).to.be.eq(NUM);
    });

    it("It should return proper address after invoking set method with address arg", async function () {
      const someAddress = solidityContract.address;
      await solidityContract.callAddress(scillaContractAddress, "setAddress", KEEP_ORIGIN, someAddress);
      let readString = await solidityContract.readAddress(scillaContractAddress, "addrField");
      expect(readString).to.be.equal(someAddress);
    });

    it("It should return proper unsigned integer value for an immutable field", async function () {
      let readRes = await solidityContract.readUint(scillaContractAddress, "immutableUintField");
      expect(readRes).to.be.eq(IMMUTABLE_UINT);
    });

    it("It should return proper integer value for an immutable field", async function () {
      let readRes = await solidityContract.readInt(scillaContractAddress, "immutableIntField");
      expect(readRes).to.be.eq(IMMUTABLE_INT);
    });

    it("It should return proper string value for an immutable field", async function () {
      let readRes = await solidityContract.readString(scillaContractAddress, "immutableStringField");
      expect(readRes).to.be.eq(IMMUTABLE_STRING);
    });

    it("It should return proper address value for an immutable field", async function () {
      let readRes = await solidityContract.readAddress(scillaContractAddress, "immutableAddressField");
      expect(readRes).to.be.eq(addr1);
    });

    it("Should fail to read uint if field name is not available", async function () {
      try {
        await solidityContract.readUint(scillaContractAddress, "nonExistant");
        expect(false).to.be.true;
      } catch (error) {
        expect(true).to.be.true;
      }
    });

    it("Should fail to read uint if contract address is not valid", async function () {
      try {
        await solidityContract.readUint("0x123456", "immutableUintField");
        expect(false).to.be.true;
      } catch (error) {
        expect(true).to.be.true;
      }
    });
  });

  // Non-existing keys

  it("It should return revert while reading non-existing  field", async function () {
    expect(solidityContract.readString(scillaContractAddress, "UknownField")).to.be.reverted;
  });

  it("It should return 0 while reading non-existing key in simpleMap", async function () {
    const VAL = 0;
    let readRes = await solidityContract.readSimpleMap(scillaContractAddress, "simpleMap", addr2);
    expect(readRes.toNumber()).to.be.eq(VAL);
  });

  it("It should return 0 while reading non-existing key in nestedMap", async function () {
    const VAL = 0;
    let readRes = await solidityContract.readNestedMap(scillaContractAddress, "nestedMap", addr2, addr1);
    expect(readRes.toNumber()).to.be.eq(VAL);
  });
});
