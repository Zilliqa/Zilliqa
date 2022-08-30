const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract with payable constructor", function () {
    describe("When ethers.js is used", function() {
        let contract;
        let INITIAL_BALANCE = 10;

        before(async function() {
            const Contract = await ethers.getContractFactory("WithPayableConstructor")
            contract = await Contract.deploy({
                value: INITIAL_BALANCE
            })
        })

        it("Should be deployed successfully", async function() {
            expect(contract.address).exist;
        })

        it("Should return 10 when balance view function is called", async function() {
            expect(await contract.balance()).to.be.eq(INITIAL_BALANCE)
        })

        it("Should return default signer when owner view function is called", async function() {
            expect(await contract.owner()).to.be.eq(await ethers.provider.getSigner().getAddress())
        })
    });

    describe("When web3.js is used", function() {
        // TODO
    });
})
