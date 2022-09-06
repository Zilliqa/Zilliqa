const { expect } = require("chai");
const { ethers } = require("hardhat");
const zilliqa = require('../helper/ZilliqaHelper');
const web3_helper = require('../helper/Web3Helper')

describe("Contract Deployment", function () {
    describe("Contract with zero parameter constructor", function () {
        describe("When ethers.js is used", function () {
            let contract;
            before(async function () {
                const Contract = await ethers.getContractFactory("ZeroParamConstructor")
                contract = await Contract.deploy()
            })

            it("Should be deployed successfully", async function () {
                expect(contract.address).exist;
            })

            it("Should return 123 when number view function is called", async function () {
                expect(await contract.number()).to.be.eq(123)
            })
        });

        describe("When Zilliqa Helper is used", function() {
            let contract;
            before(async function () {
                if (!hre.isZilliqaNetworkSelected()) {
                    this.skip()
                }

                contract = await zilliqa.deployContract("ZeroParamConstructor")
            })

            it("Should be deployed successfully", async function () {
                expect(contract._address).exist;
            })

            it("Should return 123 when number view function is called", async function () {
                expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(123))
            })
        })

        describe("When web3.js is used", function () {
            let contract;
            before(async function () {
                contract = await web3_helper.deploy("ZeroParamConstructor")
            })

            it("Should be deployed successfully", async function () {
                expect(contract.options.address).exist;
            })

            it("Should return 123 when number view function is called", async function () {
                expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(123))
            })
        });
    })

    describe("Contract with one parameter constructor", function () {
        describe("When ethers.js is used", function () {
            describe("When constructor parameter is a uint256", async function () {
                let contract;
                let INITIAL_NUMBER = 100;
                before(async function () {
                    const Contract = await ethers.getContractFactory("WithUintConstructor")
                    contract = await Contract.deploy(INITIAL_NUMBER)
                })

                it("Should be deployed successfully", async function () {
                    expect(contract.address).exist;
                })

                it("Should return 100 when number view function is called", async function () {
                    expect(await contract.number()).to.be.eq(INITIAL_NUMBER)
                })
            })

            describe("When constructor parameter is a string", async function () {
                let contract;
                let INITIAL_NAME = "Zilliqa";
                before(async function () {
                    const Contract = await ethers.getContractFactory("WithStringConstructor")
                    contract = await Contract.deploy(INITIAL_NAME)
                })

                it("Should be deployed successfully", async function () {
                    expect(contract.address).exist;
                })

                it("Should return Zilliqa when name view function is called", async function () {
                    expect(await contract.name()).to.be.eq(INITIAL_NAME)
                })
            })

            // TODO
            describe("When constructor parameter is an address", async function () {
                it("Should be deployed successfully")
                it("Should return state accordingly")
            })

            // TODO
            describe("When constructor parameter is an enum", async function () {
                it("Should be deployed successfully")
                it("Should return state accordingly")
            })
        });

        describe("When Zilliqa Helper is used", function () {
            before(async function () {
                if (!hre.isZilliqaNetworkSelected()) {
                    this.skip()
                }
            })

            describe("When constructor parameter is a uint256", async function () {
                let contract;
                let INITIAL_NUMBER = 100;
                before(async function () {
                    contract = await zilliqa.deployContract("WithUintConstructor", {
                        constructorArgs: [INITIAL_NUMBER]
                    })
                })

                it("Should be deployed successfully", async function () {
                    expect(contract._address).exist;
                })

                it("Should return 100 when number view function is called", async function () {
                    expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(INITIAL_NUMBER))
                })
            })

            describe("When constructor parameter is a string", async function () {
                let contract;
                let INITIAL_NAME = "Zilliqa";
                before(async function () {
                    contract = await zilliqa.deployContract("WithStringConstructor", {
                        constructorArgs: [INITIAL_NAME]
                    })
                })

                it("Should be deployed successfully", async function () {
                    expect(contract._address).exist;
                })

                it("Should return Zilliqa when name view function is called", async function () {
                    expect(await contract.methods.name().call()).to.be.eq(INITIAL_NAME)
                })
            })

            // TODO
            describe("When constructor parameter is an address", async function () {
                it("Should be deployed successfully")
                it("Should return state accordingly")
            })

            // TODO
            describe("When constructor parameter is an enum", async function () {
                it("Should be deployed successfully")
                it("Should return state accordingly")
            })
        });


        describe("When web3.js is used", function () {
            describe("When constructor parameter is a uint256", async function () {
                let contract;
                let INITIAL_NUMBER = 100;
                before(async function () {
                    contract = await web3_helper.deploy("WithUintConstructor", INITIAL_NUMBER)
                })

                it("Should be deployed successfully", async function () {
                    expect(contract.options.address).exist;
                })

                it("Should return 100 when number view function is called", async function () {
                    expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(INITIAL_NUMBER))
                })
            })
            // TODO add the rest
        });
    })

    describe("Contract with multi-parameter constructor", function () {
        describe("When ethers.js is used", function () {
            let contract;
            let NAME = "Zilliqa"
            let NUMBER = 100;
            before(async function () {
                const Contract = await ethers.getContractFactory("MultiParamConstructor")
                contract = await Contract.deploy(NAME, NUMBER)
            })

            it("Should be deployed successfully", async function () {
                expect(contract.address).exist;
            })

            it("Should return 100 when number view function is called", async function () {
                expect(await contract.number()).to.be.eq(NUMBER)
            })

            it("Should return Zilliqa when name view function is called", async function () {
                expect(await contract.name()).to.be.eq(NAME)
            })
        });

        describe("When Zilliqa Helper is used", function () {
            let contract;
            let NAME = "Zilliqa"
            let NUMBER = 100;
            before(async function () {
                if (!hre.isZilliqaNetworkSelected()) {
                    this.skip()
                }
                contract = await zilliqa.deployContract("MultiParamConstructor", {
                    constructorArgs: [NAME, NUMBER]
                })
            })

            it("Should be deployed successfully", async function () {
                expect(contract._address).exist;
            })

            it("Should return 100 when number view function is called", async function () {
                expect(await contract.methods.number().call()).to.be.eq(ethers.BigNumber.from(NUMBER))
            })

            it("Should return Zilliqa when name view function is called", async function () {
                expect(await contract.methods.name().call()).to.be.eq(NAME)
            })
        });

        describe("When web3.js is used", function () {
            // TODO
        });
    })

    describe("Contract with payable constructor", function () {
        describe("When ethers.js is used", function () {
            let contract;
            let INITIAL_BALANCE = 10;

            before(async function () {
                const Contract = await ethers.getContractFactory("WithPayableConstructor")
                contract = await Contract.deploy({
                    value: INITIAL_BALANCE
                })
            })

            it("Should be deployed successfully", async function () {
                expect(contract.address).exist;
            })

            it("Should return 10 when balance view function is called", async function () {
                expect(await contract.balance()).to.be.eq(INITIAL_BALANCE)
            })

            it("Should return default signer when owner view function is called", async function () {
                expect(await contract.owner()).to.be.eq(await ethers.provider.getSigner().getAddress())
            })
        });

        describe("When Zilliqa Helper is used", function () {
            let INITIAL_BALANCE = 10;
            let contract;

            before(async function () {
                contract = await zilliqa.deployContract("WithPayableConstructor", {
                    value: INITIAL_BALANCE
                })
            })

            it("Should be deployed successfully", async function () {
                expect(contract._address).exist;
            })

            it("Should return 10 when balance view function is called", async function () {
                expect(await contract.methods.balance().call()).to.be.eq(ethers.BigNumber.from(INITIAL_BALANCE))
            })

            it("Should return default signer when owner view function is called", async function () {
                expect(await contract.methods.owner().call()).to.be.eq(await ethers.provider.getSigner(0).getAddress())
            })
        });

        describe("When web3.js is used", function () {
            // TODO
        });
    })
})
