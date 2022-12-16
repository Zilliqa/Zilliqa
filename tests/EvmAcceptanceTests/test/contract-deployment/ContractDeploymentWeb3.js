const {expect} = require("chai");
const {web3} = require("hardhat");
const parallelizer = require("../../helper/Parallelizer");

describe("Contract Deployment", function () {
  describe("Contract with zero parameter constructor", function () {
    let contract;
    before(async function () {
      contract = await parallelizer.deployContractWeb3("ZeroParamConstructor");
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract.options.address).to.be.properAddress;
    });

    it("Should return 123 when number view function is called", async function () {
      expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(123));
    });
  });

  describe("Contract with one parameter constructor", function () {
    describe("When constructor parameter is a uint256", function () {
      let contract;
      const INITIAL_NUMBER = 100;

      before(async function () {
        contract = await parallelizer.deployContractWeb3("WithUintConstructor", {}, INITIAL_NUMBER);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(INITIAL_NUMBER));
      });
    });
    describe("When constructor parameter is a string", function () {
      let contract;
      let INITIAL_NAME = "Zilliqa";
      before(async function () {
        contract = await parallelizer.deployContractWeb3("WithStringConstructor", {}, INITIAL_NAME);
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.methods.name().call()).to.be.eq(INITIAL_NAME);
      });
    });
    describe("When constructor parameter is an address", function () {
      let contract;
      let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      before(async function () {
        contract = await parallelizer.deployContractWeb3("WithAddressConstructor", {}, ADDRESS);
      });
      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return constructor address when ctorAddress view function is called", async function () {
        expect(await contract.methods.someAddress().call()).to.be.eq(ADDRESS);
      });
    });
    describe("When constructor parameter is an enum", function () {
      let contract;
      let ENUM = "1";
      before(async function () {
        contract = await parallelizer.deployContractWeb3("WithEnumConstructor", {}, ENUM);
      });
      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract.options.address).to.be.properAddress;
      });
      it("Should return constructor enum when someEnum view function is called", async function () {
        expect(await contract.methods.someEnum().call()).to.be.eq(ENUM);
      });
    });
  });

  describe("Contract with multi-parameter constructor", function () {
    let contract;
    let NAME = "Zilliqa";
    let NUMBER = 100;

    before(async function () {
      contract = await parallelizer.deployContractWeb3("MultiParamConstructor", {}, NAME, NUMBER);
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract.options.address).to.be.properAddress;
    });

    it("Should return 100 when number view function is called", async function () {
      expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(NUMBER));
    });

    it("Should return Zilliqa when name view function is called", async function () {
      expect(await contract.methods.name().call()).to.be.eq(NAME);
    });
  });

  describe("Contract with payable constructor", function () {
    let contract;
    let INITIAL_BALANCE = 10;
    const gasLimit = "350000";

    before(async function () {
      contract = await parallelizer.deployContractWeb3("WithPayableConstructor", {value: INITIAL_BALANCE});
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract.options.address).to.be.properAddress;
    });

    it("Should return 10 when balance view function is called", async function () {
      expect(await contract.methods.balance().call()).to.be.eq(web3.utils.toBN(INITIAL_BALANCE));
    });

    it("Should return Zilliqa when name view function is called", async function () {
      expect(await contract.methods.owner().call()).to.be.eq(contract.options.from);
    });
  });
});
