const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract with setter and getter functions", function () {
    describe("When ethers.js is used", function() {
        let contract;
        before(async function() {
            const Contract = await ethers.getContractFactory("WithSettersAndGetters")
            contract = await Contract.deploy()
        })

        describe("When public setter function is called", async function() {
            it("Should set uint256 internal state correctly", async function() {
                let TO_BE_SET = 100
                await contract.setNumber(TO_BE_SET)
                expect(await contract.number()).to.be.eq(TO_BE_SET)
            })

            it("Should set string internal state correctly", async function() {
                let TO_BE_SET = "Zilliqa"
                await contract.setName(TO_BE_SET)
                expect(await contract.name()).to.be.eq(TO_BE_SET)
            })
  
            // TODO
            it("Should set enum internal state correctly")
            it("Should set address internal state correctly")
        })

        describe("When external view function is called", async function() {
            it("Should return correct value when uint256 view function is called", async function() {
                expect(await contract.getNumberExternal()).to.be.eq(100)
            })

            it("Should return correct value when string view function is called", async function() {
                expect(await contract.getNameExternal()).to.be.eq("Zilliqa")
            })

            // TODO: external pure functions with string, address, enum, tuple return types
        })

        describe("When external setter function is called", async function() {
            it("Should set uint256 internal state correctly", async function() {
                let TO_BE_SET = 100
                await contract.setNumberExternal(TO_BE_SET)
                expect(await contract.number()).to.be.eq(TO_BE_SET)
            })
    
            it("Should set string internal state correctly", async function() {
                let TO_BE_SET = "Zilliqa"
                await contract.setNameExternal(TO_BE_SET)
                expect(await contract.name()).to.be.eq(TO_BE_SET)
            })

            // TODO
            it("Should set enum internal state correctly")
            it("Should set address internal state correctly")
        })

        describe("When public pure function is called", async function() {
            it("Should return 1 when getOne() is called", async function() {
                expect(await contract.getOne()).to.be.eq(1)
            })
    
            // TODO: public pure functions with string, address, enum, tuple return types
        })

        describe("When external pure function is called", async function() {
            it("Should return 2 when getTwo() is called", async function() {
                expect(await contract.getTwo()).to.be.eq(2)
            })

            // TODO: external pure functions with string, address, enum, tuple return types
        })

        describe("When public view function is called", async function() {
            it("Should return 1 when getOne() is called", async function() {
                expect(await contract.getOne()).to.be.eq(1)
            })
    
            // TODO: public pure functions with string, address, enum, tuple return types
        })

    });

    describe("When web3.js is used", function() {
        // TODO
    });
})
