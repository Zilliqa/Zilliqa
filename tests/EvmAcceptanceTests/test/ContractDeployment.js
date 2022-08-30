const { expect } = require("chai");
const { ZilliqaHelper } = require('../helper/ZilliqaHelper');

// GIVEN
// WHEN
// THEN
// GIVEN A  zero-argument 
describe("Contract Deployment with web3.js", function () {
    describe("When it's being deployed using an externally owned account", function() {
        it("Should be deployed successfully when initial fund is provided")
        it("Should be deployed successfully when constructor has no argument")
        describe("with one argument to constructor", function() {
            it("Should be deployed successfully and state should be changed correctly when argument type is uint256")
            it("Should be deployed successfully and state should be changed correctly when argument type is string")
            it("Should be deployed successfully and state should be changed correctly when argument type is address")
            it("Should be deployed successfully and state should be changed correctly when argument type is enum")
        })
        describe("with two arguments to constructor", function() {
            it("Should be deployed successfully and states should be changed correctly when when to argument types are identical")
            it("Should be deployed successfully and states should be changed correctly when when to argument types are different")
        })
    });
    describe("When it's being deployed using a contract account", function() {
        it("Should be deployed successfully when initial fund is provided")
        it("Should be deployed successfully when constructor has no argument")
        describe("with one argument to constructor", function() {
            it("Should be deployed successfully and state should be changed correctly when argument type is uint256")
            it("Should be deployed successfully and state should be changed correctly when argument type is string")
            it("Should be deployed successfully and state should be changed correctly when argument type is address")
            it("Should be deployed successfully and state should be changed correctly when argument type is enum")
        })
        describe("with two arguments to constructor", function() {
            it("Should be deployed successfully and states should be changed correctly when when to argument types are identical")
            it("Should be deployed successfully and states should be changed correctly when when to argument types are different")
        })
    });
})

describe("Contract Deployment with ethers.js", function () {
    describe("When it's being deployed using an externally owned account", function() {
        it("Should be deployed successfully when initial fund is provided")
        it("Should be deployed successfully when constructor has no argument")
        describe("with one argument to constructor", function() {
            it("Should be deployed successfully and state should be changed correctly when argument type is uint256")
            it("Should be deployed successfully and state should be changed correctly when argument type is string")
            it("Should be deployed successfully and state should be changed correctly when argument type is address")
            it("Should be deployed successfully and state should be changed correctly when argument type is enum")
        })
        describe("with two arguments to constructor", function() {
            it("Should be deployed successfully and states should be changed correctly when when to argument types are identical")
            it("Should be deployed successfully and states should be changed correctly when when to argument types are different")
        })
    });
    describe("When it's being deployed using a contract account", function() {
        it("Should be deployed successfully when initial fund is provided")
        it("Should be deployed successfully when constructor has no argument")
        describe("with one argument to constructor", function() {
            it("Should be deployed successfully and state should be changed correctly when argument type is uint256")
            it("Should be deployed successfully and state should be changed correctly when argument type is string")
            it("Should be deployed successfully and state should be changed correctly when argument type is address")
            it("Should be deployed successfully and state should be changed correctly when argument type is enum")
        })
        describe("with two arguments to constructor", function() {
            it("Should be deployed successfully and states should be changed correctly when when to argument types are identical")
            it("Should be deployed successfully and states should be changed correctly when when to argument types are different")
        })
    });
})
    // const helper = new ZilliqaHelper()

    // it("Testing deployment of a contract and calling one of its functions", async function() {
    //     const contract = await helper.deployContract("Storage")
    //     const TEST_VALUE = 123
    //     await contract.methods.store(TEST_VALUE).send()
    //     const state = await helper.getStateAsNumber(contract._address, 0)
    //     expect(state).to.be.equal(TEST_VALUE)
    // });

    // it("Testing deployment of a contract using its constructor with custom parameter", async function() {
    //     const contract = await helper.deployContract("WithStringConstructor", {
    //         constructorArgs: ["saeed"]
    //     })

    //     const state = await helper.getStateAsString(contract._address, 0)
    //     expect(state).to.be.equal("saeed")
    // });

    // it("Testing deployment of a contract with multiple parameter sent to its constructor", async function() {
    //     const contract = await helper.deployContract("MultiParamConstructor", {
    //         constructorArgs: ["saeed", 123]
    //     })

    //     const name = await helper.getStateAsString(contract._address, 0)
    //     expect(name).to.be.equal("saeed")

    //     const number = await helper.getStateAsNumber(contract._address, 1)
    //     expect(number).to.be.equal(123)
 
    //     const number2 = await contract.methods.number().call()
    //     expect(number2).to.be.equal('123')
    // });

    // it("Testing deployment of a contract with an initial fund", async function() {
    //     const INITIAL_FUND = 300;

    //     const contract = await helper.deployContract("ParentContract", {
    //         value: INITIAL_FUND
    //     })

    //     const result = await contract.methods.getPaidValue().call()
    //     expect(Number(result)).to.be.equal(INITIAL_FUND)
    // });
