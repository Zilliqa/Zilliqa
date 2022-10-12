const {expect} = require("chai");
const {ethers, web3} = require("hardhat");
const general_helper = require("../helper/GeneralHelper");
const web3_helper = require("../helper/Web3Helper");
const zilliqa_helper = require("../helper/ZilliqaHelper");

describe("Contract Deployment", function () {
  describe("Contract with zero parameter constructor", function () {
    describe("When ethers.js is used", function () {
      let contract;
      before(async function () {
        const Contract = await ethers.getContractFactory("ZeroParamConstructor");
        contract = await Contract.deploy();
      });

      it("Should be deployed successfully", async function () {
        expect(contract.address).to.be.properAddress;
      });

      it("Should return 123 when number view function is called", async function () {
        expect(await contract.number()).to.be.eq(123);
      });
    });

    describe("When Zilliqa Helper is used", function () {
      let contract;
      before(async function () {
        if (!general_helper.isZilliqaNetworkSelected()) {
          this.skip();
        }

        contract = await zilliqa_helper.deployContract("ZeroParamConstructor");
      });

      it("Should be deployed successfully", async function () {
        expect(contract._address).to.be.properAddress;
      });

      it("Should return 123 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(123));
      });
    });

    describe("When web3.js is used", function () {
      let contract;
      before(async function () {
        contract = await web3_helper.deploy("ZeroParamConstructor", {gasLimit: 220000});
      });

      it("Should be deployed successfully", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return 123 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(123));
      });
    });
  });

  describe("Contract with one parameter constructor", function () {
    describe("When ethers.js is used", function () {
      describe("When constructor parameter is a uint256", function () {
        let contract;
        let INITIAL_NUMBER = 100;
        before(async function () {
          const Contract = await ethers.getContractFactory("WithUintConstructor");
          contract = await Contract.deploy(INITIAL_NUMBER);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.address).to.be.properAddress;
        });

        it("Should return 100 when number view function is called", async function () {
          expect(await contract.number()).to.be.eq(INITIAL_NUMBER);
        });
      });

      describe("When constructor parameter is a string", function () {
        let contract;
        let INITIAL_NAME = "Zilliqa";
        before(async function () {
          const Contract = await ethers.getContractFactory("WithStringConstructor");
          contract = await Contract.deploy(INITIAL_NAME);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.address).to.be.properAddress;
        });

        it("Should return Zilliqa when name view function is called", async function () {
          expect(await contract.name()).to.be.eq(INITIAL_NAME);
        });
      });

      describe("When constructor parameter is an address", function () {
        let contract;
        let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
        before(async function () {
          const Contract = await ethers.getContractFactory("WithAddressConstructor");
          contract = await Contract.deploy(ADDRESS);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.address).to.be.properAddress;
        });

        it("Should return the address state correctly", async function () {
          expect(await contract.someAddress()).to.be.eq(ADDRESS);
        });
      });

      describe("When constructor parameter is an enum", function () {
        let contract;
        let ENUM = 1;
        before(async function () {
          const Contract = await ethers.getContractFactory("WithEnumConstructor");
          contract = await Contract.deploy(ENUM);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.address).to.be.a.properAddress;
        });

        it("Should return enum state correctly", async function () {
          expect(await contract.someEnum()).to.be.eq(ENUM);
        });
      });
    });

    describe("When Zilliqa Helper is used", function () {
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

        it("Should be deployed successfully", async function () {
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

        it("Should be deployed successfully", async function () {
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

        it("Should be deployed successfully", async function () {
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

        it("Should be deployed successfully", async function () {
          expect(contract._address).to.be.a.properAddress;
        });

        it("Should return state accordingly", async function () {
          expect(await contract.methods.someEnum().call()).to.be.eq(ENUM);
        });
      });
    });

    describe("When web3.js is used", function () {
      describe("When constructor parameter is a uint256", function () {
        let contract;
        const INITIAL_NUMBER = 100;
        const gasLimit = 220000;

        before(async function () {
          contract = await web3_helper.deploy("WithUintConstructor", {gasLimit}, INITIAL_NUMBER);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.options.address).to.be.properAddress;
        });

        it("Should return 100 when number view function is called", async function () {
          expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(INITIAL_NUMBER));
        });
      });
      describe("When constructor parameter is a string", function () {
        let contract;
        let INITIAL_NAME = "Zilliqa";
        const gasLimit = 220000;
        before(async function () {
          contract = await web3_helper.deploy("WithStringConstructor", {gasLimit}, INITIAL_NAME);
        });

        it("Should be deployed successfully", async function () {
          expect(contract.options.address).to.be.properAddress;
        });

        it("Should return Zilliqa when name view function is called", async function () {
          expect(await contract.methods.name().call()).to.be.eq(INITIAL_NAME);
        });
      });
      describe("When constructor parameter is an address", function () {
        let contract;
        let ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
        const gasLimit = "220000";
        before(async function () {
          contract = await web3_helper.deploy("WithAddressConstructor", {gasLimit}, ADDRESS);
        });
        it("Should be deployed successfully", async function () {
          expect(contract.options.address).to.be.properAddress;
        });

        it("Should return constructor address when ctorAddress view function is called", async function () {
          expect(await contract.methods.someAddress().call()).to.be.eq(ADDRESS);
        });
      });
      describe("When constructor parameter is an enum", function () {
        let contract;
        let ENUM = "1";
        const gasLimit = "220000";
        before(async function () {
          contract = await web3_helper.deploy("WithEnumConstructor", {gasLimit}, ENUM);
        });
        it("Should be deployed successfully", async function () {
          expect(contract.options.address).to.be.properAddress;
        });
        it("Should return constructor enum when someEnum view function is called", async function () {
          expect(await contract.methods.someEnum().call()).to.be.eq(ENUM);
        });
      });
    });
  });

  describe("Contract with multi-parameter constructor", function () {
    describe("When ethers.js is used", function () {
      let contract;
      let NAME = "Zilliqa";
      let NUMBER = 100;
      before(async function () {
        const Contract = await ethers.getContractFactory("MultiParamConstructor");
        contract = await Contract.deploy(NAME, NUMBER);
      });

      it("Should be deployed successfully", async function () {
        expect(contract.address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await contract.number()).to.be.eq(NUMBER);
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.name()).to.be.eq(NAME);
      });
    });

    describe("When Zilliqa Helper is used", function () {
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

      it("Should be deployed successfully", async function () {
        expect(contract._address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(NUMBER));
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.methods.name().call()).to.be.eq(NAME);
      });
    });

    describe("When web3.js is used", function () {
      let contract;
      let NAME = "Zilliqa";
      let NUMBER = 100;
      const gasLimit = "350000";

      before(async function () {
        contract = await web3_helper.deploy("MultiParamConstructor", {gasLimit}, NAME, NUMBER);
      });

      it("Should be deployed successfully", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return 100 when number view function is called", async function () {
        expect(await contract.methods.number().call()).to.be.eq(web3.utils.toBN(NUMBER));
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.methods.name().call()).to.be.eq(NAME);
      });
    });
  });

  describe("Contract with payable constructor", function () {
    describe("When ethers.js is used", function () {
      let contract;
      let INITIAL_BALANCE = 10;

      before(async function () {
        const Contract = await ethers.getContractFactory("WithPayableConstructor");
        contract = await Contract.deploy({
          value: INITIAL_BALANCE
        });
      });

      it("Should be deployed successfully", async function () {
        expect(contract.address).to.be.properAddress;
      });

      it("Should return 10 when balance view function is called", async function () {
        expect(await contract.balance()).to.be.eq(INITIAL_BALANCE);
      });

      it("Should return default signer when owner view function is called", async function () {
        expect(await contract.owner()).to.be.eq(await ethers.provider.getSigner().getAddress());
      });
    });

    describe("When Zilliqa Helper is used", function () {
      let INITIAL_BALANCE = 10;
      let contract;

      before(async function () {
        contract = await zilliqa_helper.deployContract("WithPayableConstructor", {
          value: INITIAL_BALANCE
        });
      });

      it("Should be deployed successfully", async function () {
        expect(contract._address).to.be.properAddress;
      });

      it("Should return 10 when balance view function is called", async function () {
        expect(await contract.methods.balance().call()).to.be.eq(ethers.BigNumber.from(INITIAL_BALANCE));
      });

      it("Should return default signer when owner view function is called", async function () {
        expect(await contract.methods.owner().call()).to.be.eq(await ethers.provider.getSigner(0).getAddress());
      });
    });

    describe("When web3.js is used", function () {
      let contract;
      let INITIAL_BALANCE = 10;
      const gasLimit = "350000";

      before(async function () {
        contract = await web3_helper.deploy("WithPayableConstructor", {gasLimit, value: INITIAL_BALANCE});
      });

      it("Should be deployed successfully", async function () {
        expect(contract.options.address).to.be.properAddress;
      });

      it("Should return 10 when balance view function is called", async function () {
        expect(await contract.methods.balance().call()).to.be.eq(web3.utils.toBN(INITIAL_BALANCE));
      });

      it("Should return Zilliqa when name view function is called", async function () {
        expect(await contract.methods.owner().call()).to.be.eq(web3_helper.getPrimaryAccountAddress());
      });
    });
  });
});
