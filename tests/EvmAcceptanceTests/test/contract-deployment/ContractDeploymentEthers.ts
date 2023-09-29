import {expect} from "chai";
import hre, {ethers} from "hardhat";
import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {Contract} from "ethers";

describe("Contract Deployment using Ethers.js #parallel", function () {
  describe("Contract with zero parameter constructor", function () {
    let signer: SignerWithAddress;
    let nonceBeforeDeploy: number;
    let zeroParamConstructor: Contract;
    before(async function () {
      signer = hre.allocateEthSigner();
      nonceBeforeDeploy = await ethers.provider.getTransactionCount(signer.getAddress());
      zeroParamConstructor = await hre.deployContractWithSigner("ZeroParamConstructor", signer);
    });

    after(() => {
      hre.releaseEthSigner(signer);
    });

    it("Should be deployed successfully @block-1 [@transactional]", async function () {
      expect(zeroParamConstructor.address).to.be.properAddress;
    });

    it("Should have incremented the account nonce by 1 @block-1", async function () {
      const count = await ethers.provider.getTransactionCount(signer.getAddress());
      expect(count).to.be.eq(nonceBeforeDeploy + 1);
    });

    it("Should return 123 when number view function is called @block-1", async function () {
      expect(await zeroParamConstructor.number()).to.be.eq(123);
    });
  });

  describe("Contract with one parameter constructor", function () {
    describe("When constructor parameter is a uint256", function () {
      let INITIAL_NUMBER = 100;
      let withUintConstructor: Contract;
      before(async function () {
        withUintConstructor = await hre.deployContract("WithUintConstructor", INITIAL_NUMBER);
      });

      it("Should be deployed successfully @block-1 [@transactional]", async function () {
        expect(withUintConstructor.address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called @block-1", async function () {
        expect(await withUintConstructor.number()).to.be.eq(INITIAL_NUMBER);
      });
    });

    describe("When constructor parameter is a string", function () {
      let contract;
      let INITIAL_NAME = "Zilliqa";
      let withStringConstructor: Contract;
      before(async function () {
        withStringConstructor = await hre.deployContract("WithStringConstructor", INITIAL_NAME);
      });

      it("Should be deployed successfully @block-1 [@transactional]", async function () {
        expect(withStringConstructor.address).to.be.properAddress;
      });

      it("Should return Zilliqa when name view function is called @block-1", async function () {
        expect(await withStringConstructor.name()).to.be.eq(INITIAL_NAME);
      });
    });

    describe("When constructor parameter is an address", function () {
      let contract;
      let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      let withAddressConstructor: Contract;
      before(async function () {
        withAddressConstructor = await hre.deployContract("WithAddressConstructor", ADDRESS);
      });

      it("Should be deployed successfully @block-1 [@transactional]", async function () {
        expect(withAddressConstructor.address).to.be.properAddress;
      });

      it("Should return the address state correctly @block-1", async function () {
        expect(await withAddressConstructor.someAddress()).to.be.eq(ADDRESS);
      });
    });

    describe("When constructor parameter is an enum", function () {
      let ENUM = 1;
      let withEnumConstructor: Contract;
      before(async function () {
        withEnumConstructor = await hre.deployContract("WithEnumConstructor", ENUM);
      });

      it("Should be deployed successfully @block-1 [@transactional]", async function () {
        expect(withEnumConstructor.address).to.be.a.properAddress;
      });

      it("Should return enum state correctly @block-1", async function () {
        expect(await withEnumConstructor.someEnum()).to.be.eq(ENUM);
      });
    });
  });

  describe("Contract with multi-parameter constructor", function () {
    let NAME = "Zilliqa";
    let NUMBER = 100;
    let withMultiParamConstructor: Contract;
    before(async function () {
      withMultiParamConstructor = await hre.deployContract("MultiParamConstructor", NAME, NUMBER);
    });

    it("Should be deployed successfully  @block-1 [@transactional]", async function () {
      expect(withMultiParamConstructor.address).to.be.properAddress;
    });

    it("Should return 100 when number view function is called @block-1", async function () {
      expect(await withMultiParamConstructor.number()).to.be.eq(NUMBER);
    });

    it("Should return Zilliqa when name view function is called @block-1", async function () {
      expect(await withMultiParamConstructor.name()).to.be.eq(NAME);
    });
  });

  describe("Contract with payable constructor", function () {
    let INITIAL_BALANCE = 10;
    let withPayableConstructor: Contract;

    before(async function () {
      withPayableConstructor = await hre.deployContract("WithPayableConstructor", {
        value: INITIAL_BALANCE
      });
    });

    it("Should be deployed successfully @block-1 [@transactional]", async function () {
      expect(withPayableConstructor.address).to.be.properAddress;
    });

    it("Should return 10 when balance view function is called @block-1", async function () {
      expect(await withPayableConstructor.balance()).to.be.eq(INITIAL_BALANCE);
    });

    it("Should return default signer when owner view function is called @block-1", async function () {
      expect(await withPayableConstructor.owner()).to.be.eq(await withPayableConstructor.signer.getAddress());
    });
  });
});
