const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract with zero parameter constructor", function () {
    describe("When ethers.js is used", function() {
        let contract;
        before(async function() {
            const Contract = await ethers.getContractFactory("ZeroParamConstructor")
            contract = await Contract.deploy()
        })

        it("Should be deployed successfully", async function() {
            expect(contract.address).exist;
        })

        it("Should return 123 when number view function is called", async function() {
            expect(await contract.number()).to.be.eq(123)
        })
    });

    describe("When web3.js is used", function() {
        // TODO
    });
})
