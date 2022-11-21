const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../helper/Web3Helper");

// Reference: https://dev.to/yongchanghe/tutorial-using-create2-to-predict-the-contract-address-before-deploying-12cb

describe("Create2 instruction", function () {
  const INITIAL_FUND = 1_000_000;
  let contract;
  let createContract;

  before(async function () {
    const Contract = await ethers.getContractFactory("Create2Factory");
    contract = await Contract.deploy();
  });

////  before(async function () {
//    //const Contract = await ethers.getContractFactory("Create2");
//    //createContract = await Contract.deploy({value: INITIAL_FUND});
//  //});
//
  describe("Should be able to call create2 contract", function () {
    // Make sure parent contract is available for child to be called
    before(async function () {
      //const gasLimit = "750000";
      //const amountPaid = web3.utils.toBN(web3.utils.toWei("300", "gwei"));

      //const amountPaid = web3.utils.toBN(web3.utils.toWei("300", "gwei"));
      //contract = await web3_helper.deploy("Create2Factory", {gasLimit, value: amountPaid});
      //contract = await web3_helper.deploy("Create2Factory", {gasLimit});
        //
      createContract = await web3_helper.deploy("Create2Factory");
    });

    it("Should return proper gas estimation [@transactional]", async function () {
      console.log("EEE");

      const [owner] = await ethers.getSigners();
      const SALT = 1;


      const byteCode = await createContract.methods.getBytecode(owner.address);

      console.log("Bytecode: ", byteCode);

      console.log(createContract.methods);
      const addr = await createContract.methods.getAddress(byteCode, SALT);

      console.log("new addr: ", addr);
      console.log("methods", createContract.methods);

      const deployResult = await createContract.methods.deploy(SALT).call();

      console.log("deploy result: ", deployResult);

      const answer = await createContract.methods.getTestmeTwo().call();
      console.log("the anser is: ", answer);

      //const answer2 = await createContract.methods.getTestmeTwo().call();
      //console.log("the anser is: ", answer2);
    });
  });

});
