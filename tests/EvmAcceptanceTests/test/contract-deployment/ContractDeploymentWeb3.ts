import {expect} from "chai";
import {parallelizer} from "../../helpers";
import {web3} from "hardhat";
import {Contract} from "web3-eth-contract";

describe("Contract Deployment", function () {
  describe("Contract with zero parameter constructor", function () {
    let contract: Contract;
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
      let contract: Contract;
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
      let contract: Contract;
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
      let contract: Contract;
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
      let contract: Contract;
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
    let contract: Contract;
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
    let contract: Contract;
    let INITIAL_BALANCE = web3.utils.toBN(web3.utils.toWei("1", "gwei"));

    before(async function () {
      contract = await parallelizer.deployContractWeb3("WithPayableConstructor", {value: INITIAL_BALANCE});
    });

    it("Should be deployed successfully [@transactional]", async function () {
      expect(contract.options.address).to.be.properAddress;
    });

    it("Should return 10 when balance view function is called", async function () {
      expect(await contract.methods.balance().call()).to.be.eq(INITIAL_BALANCE);
    });

    it("Should return Zilliqa when name view function is called", async function () {
      expect(await contract.methods.owner().call()).to.be.eq(contract.options.from);
    });
  });
});
