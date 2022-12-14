const {ethers} = require("hardhat");

var parallelizer = {
  signers: [],
  signerAddresses: [],
  initSigners: async function () {
    this.signers = Array.from({length: 10}, (v, k) => ethers.Wallet.createRandom().connect(ethers.provider));
    this.signerAddresses = this.signers.map((signer) => signer.address);
    const BatchTransferContract = await ethers.getContractFactory("BatchTransfer");
    const batchTransfer = await BatchTransferContract.deploy(this.signerAddresses, {value: 20_000_000});
    await batchTransfer.deployed();

    for (let index = 0; index < this.signers.length; index++) {
      console.log("khar, ", this.signerAddresses[index], await ethers.provider.getBalance(this.signerAddresses[index]));
    }
  },
  deployContract: async function (contractName, ...args) {
    const signers = await ethers.getSigners();
    const signerIndex = process.env.MOCHA_WORKER_ID % signers.length || 0;
    const signer = signers[signerIndex];
    this.xxx.push(signerIndex);
    console.log(
      `ContractName: ${contractName}, MochaID: ${process.env.MOCHA_WORKER_ID} ID: ${signerIndex}, Address: ${signer.address}`
    );
    console.log("KKKK", this.xxx);

    const Contract = await ethers.getContractFactory(contractName);
    return Contract.connect(signer).deploy(...args);
  },
  createRandomAccount: function () {
    return ethers.Wallet.createRandom().connect(ethers.provider);
  },
  getSignerForCurrentWorker: async function () {
    const signers = await ethers.getSigners();
    return signers[process.env.MOCHA_WORKER_ID % this.signers.length || 0];
  }
};

module.exports = parallelizer;
