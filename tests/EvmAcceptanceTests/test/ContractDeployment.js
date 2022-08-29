const { expect } = require("chai");
const { loadFixture } = require("@nomicfoundation/hardhat-network-helpers");
const { ZilliqaHelper } = require('../helper/ZilliqaHelper');

describe("Contract Deployment", function () {
    const helper = new ZilliqaHelper()

    it("Testing deployment of a contract and calling one of its functions", async function() {
        const contract = await helper.deployContract("Storage")
        const TEST_VALUE = 123
        await contract.methods.store(TEST_VALUE).send()
        const state = await helper.getStateAsNumber(contract._address, 0)
        expect(state).to.be.equal(TEST_VALUE)
    });

    it("Testing deployment of a contract using its constructor with custom parameter", async function() {
        const contract = await helper.deployContract("WithStringConstructor", {
            constructorArgs: ["saeed"]
        })

        const state = await helper.getStateAsString(contract._address, 0)
        expect(state).to.be.equal("saeed")
    });

    it("Testing deployment of a contract with multiple parameter sent to its constructor", async function() {
        const contract = await helper.deployContract("MultiParamConstructor", {
            constructorArgs: ["saeed", 123]
        })

        const name = await helper.getStateAsString(contract._address, 0)
        expect(name).to.be.equal("saeed")

        const number = await helper.getStateAsNumber(contract._address, 1)
        expect(number).to.be.equal(123)
 
        const number2 = await contract.methods.number().call()
        expect(number2).to.be.equal('123')
    });

    it("Testing deployment of a contract with an initial fund", async function() {
        const INITIAL_FUND = 300;

        const contract = await helper.deployContract("ParentContract", {
            value: INITIAL_FUND
        })

        const result = await contract.methods.getPaidValue().call()
        expect(Number(result)).to.be.equal(INITIAL_FUND)
    });
});
