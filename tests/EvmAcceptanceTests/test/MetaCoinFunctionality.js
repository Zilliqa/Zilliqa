const { expect } = require("chai");
const web3_helper = require('../helper/Web3Helper')

describe("MetaCoin Functionality", function () {
    let contract;
    before(async function () {
        contract = await web3_helper.deploy("MetaCoin")
    })

    it("Should be deployed successfully", async function () {
        expect(contract.options.address).exist;
    })
});