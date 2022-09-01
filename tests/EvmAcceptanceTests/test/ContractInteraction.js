const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Contract Interaction", function () {
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

        describe("When public view function is called", async function() {
            it("Should return correct value for uint256", async function() {
                expect(await contract.getOne()).to.be.eq(1)
            })
    
            // TODO: Consider string, address, enum, tuple return types
        })

        describe("When external view function is called", async function() {
            it("Should return correct value for uint256", async function() {
                expect(await contract.getNumberExternal()).to.be.eq(100)
            })

            it("Should return correct value for string", async function() {
                expect(await contract.getNameExternal()).to.be.eq("Zilliqa")
            })

            // TODO: Consider string, address, enum, tuple return types
        })


        describe("When public pure function is called", async function() {
            it("Should return correct value for uint256", async function() {
                expect(await contract.getOne()).to.be.eq(1)
            })
    
            // TODO: Consider string, address, enum, tuple return types
        })

        describe("When external pure function is called", async function() {
            it("Should return correct value for uint256", async function() {
                expect(await contract.getTwo()).to.be.eq(2)
            })

            // TODO: Consider string, address, enum, tuple return types
        })

        describe("When calling a public function that generates an event", async function() {
            it("Should have the event name in returned object", async function() {
                const tx = await contract.emitLogWithoutParam()
                const result = await tx.wait()
                expect(result.events[0].event).to.be.equal("logWithoutParam")
            })

            it("Should have the event argument in returned object when arg type is uint256", async function() {
                const tx = await contract.emitLogWithUint256Param()
                const result = await tx.wait()
                expect(result.events[0].args[0]).to.be.equal(ethers.BigNumber.from(234))
            })

            // TODO
            it("Should have the event arguments in returned object when event has multiple args")

            // TODO: Consider string, address, enum, tuple arg types
            // TODO: Calling a method on a contract that generates anonymous events with indexed parameters
            // TODO: Calling a method on a contract that generates non-anonymous events with indexed parameters
        })
    });

    describe("When web3.js is used", function() {
        // TODO
    });
})
