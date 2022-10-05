const { web3 } = require("hardhat");
const web3_helper = require("../helper/Web3Helper");
const zilliqa_helper = require("../helper/ZilliqaHelper");
assert = require('chai').assert;


describe("Revert Contract Call", function () {
  let contract;
  before(async function () {
    contract = await web3_helper.deploy("Revert");
  });


  xit("Will revert the contract when revert is called", async function () {

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
