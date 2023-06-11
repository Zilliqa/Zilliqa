import {expect} from "chai";
import {parallelizer} from "../../helpers";
import {ethers} from "hardhat";

describe("Contract Deployment using Ethers.js", function () {
  describe("Contract with zero parameter constructor", function () {
    before(async function () {
      this.signer = await parallelizer.takeSigner();
      this.nonceBeforeDeploy = await ethers.provider.getTransactionCount(this.signer.getAddress());
      this.zeroParamConstructor = await parallelizer.deployContractWithSigner(this.signer, "ZeroParamConstructor");
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(this.zeroParamConstructor.address).to.be.properAddress;
    });

    it("Should have incremented the account nonce by 1", async function () {
      const count = await ethers.provider.getTransactionCount(this.signer.getAddress());
      expect(count).to.be.eq(this.nonceBeforeDeploy + 1);
    });

    it("Should return 123 when number view function is called", async function () {
      expect(await this.zeroParamConstructor.number()).to.be.eq(123);
    });
  });

  describe("Contract with one parameter constructor", function () {
    describe("When constructor parameter is a uint256", function () {
      let contract;
      let INITIAL_NUMBER = 100;
      before(async function () {
        this.withUintConstructor = await parallelizer.deployContract("WithUintConstructor", INITIAL_NUMBER);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(this.withUintConstructor.address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await this.withUintConstructor.number()).to.be.eq(INITIAL_NUMBER);
      });
    });

    describe("When constructor parameter is a string", function () {
      let contract;
      let INITIAL_NAME = "Zilliqa";
      before(async function () {
        this.withStringConstructor = await parallelizer.deployContract("WithStringConstructor", INITIAL_NAME);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(this.withStringConstructor.address).to.be.properAddress;
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await this.withStringConstructor.name()).to.be.eq(INITIAL_NAME);
      });
    });

    describe("When constructor parameter is an address", function () {
      let contract;
      let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      before(async function () {
        this.withAddressConstructor = await parallelizer.deployContract("WithAddressConstructor", ADDRESS);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(this.withAddressConstructor.address).to.be.properAddress;
      });

      it("Should return the address state correctly", async function () {
        expect(await this.withAddressConstructor.someAddress()).to.be.eq(ADDRESS);
      });
    });

    describe("When constructor parameter is an enum", function () {
      let ENUM = 1;
      before(async function () {
        this.withEnumConstructor = await parallelizer.deployContract("WithEnumConstructor", ENUM);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(this.withEnumConstructor.address).to.be.a.properAddress;
      });

      it("Should return enum state correctly", async function () {
        expect(await this.withEnumConstructor.someEnum()).to.be.eq(ENUM);
      });
    });
  });

  describe("Contract with multi-parameter constructor", function () {
    let NAME = "Zilliqa";
    let NUMBER = 100;
    before(async function () {
      this.withMultiParamConstructor = await parallelizer.deployContract("MultiParamConstructor", NAME, NUMBER);
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(this.withMultiParamConstructor.address).to.be.properAddress;
    });

    it("Should return 100 when number view function is called", async function () {
      expect(await this.withMultiParamConstructor.number()).to.be.eq(NUMBER);
    });

    it("Should return Zilliqa when name view function is called", async function () {
      expect(await this.withMultiParamConstructor.name()).to.be.eq(NAME);
    });
  });

  describe("Contract with payable constructor", function () {
    let INITIAL_BALANCE = 10;

    before(async function () {
      this.withPayableConstructor = await parallelizer.deployContract("WithPayableConstructor", {
        value: INITIAL_BALANCE
      });
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(this.withPayableConstructor.address).to.be.properAddress;
    });

    it("Should return 10 when balance view function is called", async function () {
      expect(await this.withPayableConstructor.balance()).to.be.eq(INITIAL_BALANCE);
    });

    it("Should return default signer when owner view function is called", async function () {
      expect(await this.withPayableConstructor.owner()).to.be.eq(await this.withPayableConstructor.signer.getAddress());
    });
  });
});
