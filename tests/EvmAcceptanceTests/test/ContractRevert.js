const { expect } = require("chai");
const { ethers, web3 } = require("hardhat");
const general_helper = require("../helper/GeneralHelper");
const web3_helper = require("../helper/Web3Helper");
const zilliqa_helper = require("../helper/ZilliqaHelper");
assert = require('chai').assert;


describe("Revert Contract Deployment", function () {
  let contract;
  before(async function () {
    contract = await web3_helper.deploy("Revert");
  });


  it("Will revert the contract", async function () {

    function onCallViewFinished(receipt) {
      assert.fail("Failure: Should not be successful");
    }

    function onCallViewError(error) {
      assert.notEqual(error.data.stack.search("Reverted"), -1);
    }

    await zilliqa_helper.callView(contract, "revertCall")
      .then(onCallViewFinished, onCallViewError);
  });
})