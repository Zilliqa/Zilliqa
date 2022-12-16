const {expect} = require("chai");
const {ethers, web3} = require("hardhat");
const general_helper = require("../../helper/GeneralHelper");
const web3_helper = require("../../helper/Web3Helper");
const zilliqa_helper = require("../../helper/ZilliqaHelper");

describe("Contract Deployment", function () {
  describe("Contract with zero parameter constructor", function () {
    let contract;
    before(async function () {
      if (!general_helper.isZilliqaNetworkSelected()) {
        this.skip();
      }

      contract = await zilliqa_helper.deployContract("ZeroParamConstructor");
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract._address).to.be.properAddress;
    });

    it("Should return 123 when number view function is called", async function () {
      expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(123));
    });
  });

  describe("Contract with one parameter constructor", function () {
    before(async function () {
      if (!general_helper.isZilliqaNetworkSelected()) {
        this.skip();
      }
    });

    describe("When constructor parameter is a uint256", function () {
      let contract;
      let INITIAL_NUMBER = 100;
      before(async function () {
        contract = await zilliqa_helper.deployContract("WithUintConstructor", {
          constructorArgs: [INITIAL_NUMBER]
        });
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract._address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(INITIAL_NUMBER));
      });
    });

    describe("When constructor parameter is a string", function () {
      let contract;
      let INITIAL_NAME = "Zilliqa";
      before(async function () {
        contract = await zilliqa_helper.deployContract("WithStringConstructor", {
          constructorArgs: [INITIAL_NAME]
        });
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract._address).to.be.properAddress;
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.methods.name().call()).to.be.eq(INITIAL_NAME);
      });
    });

    describe("When constructor parameter is an address", function () {
      let contract;
      let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
      before(async function () {
        contract = await zilliqa_helper.deployContract("WithAddressConstructor", {
          constructorArgs: [ADDRESS]
        });
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract._address).to.be.a.properAddress;
      });

      it("Should return address value correctly", async function () {
        expect(await contract.methods.someAddress().call()).to.be.eq(ADDRESS);
      });
    });

    describe("When constructor parameter is an enum", function () {
      let contract;
      let ENUM = "1";

      before(async function () {
        contract = await zilliqa_helper.deployContract("WithEnumConstructor", {
          constructorArgs: [ENUM]
        });
      });

      it("Should be deployed successfully [@transactional]", async function () {
        expect(contract._address).to.be.a.properAddress;
      });

      it("Should return state accordingly", async function () {
        expect(await contract.methods.someEnum().call()).to.be.eq(ENUM);
      });
    });
  });

  describe("Contract with multi-parameter constructor", function () {
    let contract;
    let NAME = "Zilliqa";
    let NUMBER = 100;
    before(async function () {
      if (!general_helper.isZilliqaNetworkSelected()) {
        this.skip();
      }
      contract = await zilliqa_helper.deployContract("MultiParamConstructor", {
        constructorArgs: [NAME, NUMBER]
      });
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract._address).to.be.properAddress;
    });

    it("Should return 100 when number view function is called", async function () {
      expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(NUMBER));
    });

    it("Should return Zilliqa when name view function is called", async function () {
      expect(await contract.methods.name().call()).to.be.eq(NAME);
    });
  });

  describe("Contract with payable constructor", function () {
    let INITIAL_BALANCE = 10;
    let contract;

    before(async function () {
      contract = await zilliqa_helper.deployContract("WithPayableConstructor", {
        value: INITIAL_BALANCE
      });
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract._address).to.be.properAddress;
    });

    it("Should return 10 when balance view function is called", async function () {
      expect(await contract.methods.balance().call()).to.be.eq(ethers.BigNumber.from(INITIAL_BALANCE));
    });

    it("Should return default signer when owner view function is called", async function () {
      expect(await contract.methods.owner().call()).to.be.eq(await ethers.provider.getSigner(0).getAddress());
    });
  });
});
