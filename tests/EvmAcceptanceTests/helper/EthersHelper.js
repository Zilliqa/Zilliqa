const {ethers} = require("hardhat");

var ethers_helper = {
  deployContract: async function (contractName) {
    const signers = await ethers.getSigners();
    const signerIndex = process.env.MOCHA_WORKER_ID % signers.length || 0;
    const signer = signers[signerIndex];
    console.log(`ID: ${signerIndex}, Address: ${signer.address}`);

    const Contract = await ethers.getContractFactory(contractName);
    return Contract.connect(signer).deploy();
  },
  signer: async function () {
    const owners = await ethers.getSigners();
    return owners[process.env.MOCHA_WORKER_ID || 0];
  }
};

module.exports = ethers_helper;
