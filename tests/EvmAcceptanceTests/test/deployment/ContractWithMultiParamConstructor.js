const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract with multi-parameter constructor", function () {
    describe("When ethers.js is used", function() {
        let contract;
        let NAME = "Zilliqa"
        let NUMBER = 100;
        before(async function() {
            const Contract = await ethers.getContractFactory("MultiParamConstructor")
            contract = await Contract.deploy(NAME, NUMBER)
        })

        it("Should be deployed successfully", async function() {
            expect(contract.address).exist;
        })

        it("Should return 100 when number view function is called", async function() {
            expect(await contract.number()).to.be.eq(NUMBER)
        })

        it("Should return Zilliqa when name view function is called", async function() {
            expect(await contract.name()).to.be.eq(NAME)
        })
    });
})
