const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract with one parameter constructor", function () {
    describe("When ethers.js is used", function() {
        describe("When constructor parameter is a uint256", async function() {
            let contract;
            let INITIAL_NUMBER = 100;
            before(async function() {
                const Contract = await ethers.getContractFactory("WithUintConstructor")
                contract = await Contract.deploy(INITIAL_NUMBER)
            })

            it("Should be deployed successfully", async function() {
                expect(contract.address).exist;
            })
    
            it("Should return 100 when number view function is called", async function() {
                expect(await contract.number()).to.be.eq(INITIAL_NUMBER)
            })
        })

        describe("When constructor parameter is a string", async function() {
            let contract;
            let INITIAL_NAME = "Zilliqa";
            before(async function() {
                const Contract = await ethers.getContractFactory("WithStringConstructor")
                contract = await Contract.deploy(INITIAL_NAME)
            })

            it("Should be deployed successfully", async function() {
                expect(contract.address).exist;
            })
    
            it("Should return Zilliqa when name view function is called", async function() {
                expect(await contract.name()).to.be.eq(INITIAL_NAME)
            })
        })

        // TODO
        describe("When constructor parameter is an address", async function() {
            it("Should be deployed successfully")
            it("Should return state accordingly")
        })

        // TODO
        describe("When constructor parameter is an enum", async function() {
            it("Should be deployed successfully")
            it("Should return state accordingly")
        })
    });

    describe("When web3.js is used", function() {
        // TODO
    });
})
