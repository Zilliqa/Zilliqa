const {ethers, web3} = require("hardhat");

var parallelizer = {
  signers: [],
  initSigners: async function () {
    const signer = await this.getSignerForCurrentWorker();
    const newSigners = Array.from({length: 10}, (v, k) => ethers.Wallet.createRandom().connect(ethers.provider));
    const BatchTransferContract = await ethers.getContractFactory("BatchTransfer");
    const batchTransfer = await BatchTransferContract.connect(signer).deploy({value: 10_000_000_000_000});
    await batchTransfer.deployed();
    const addresses = newSigners.map((signer) => signer.address);
    await batchTransfer.batchTransfer(addresses, 1_000_000_000_000);

    this.signers.push(...newSigners);
  },
  deployContract: async function (contractName, ...args) {
    const signer = await this.takeSigner();
    const Contract = await ethers.getContractFactory(contractName);
    return Contract.connect(signer).deploy(...args);
  },
  deployContractWeb3: async function (contractName, options = {}, ...args) {
    const signer = await this.takeSigner();

    web3.eth.accounts.wallet.add(signer.privateKey);
    const contractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new web3.eth.Contract(contractRaw.abi);
    const gasPrice = options.gasPrice || (await web3.eth.getGasPrice());
    const gasLimit = options.gasLimit || 210_000;

    const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
      from: signer.address,
      gas: gasLimit,
      gasPrice: gasPrice,
      value: options.value ?? 0
    });

    deployedContract.options.from = signer.address;
    deployedContract.options.gas = gasLimit;
    return deployedContract;
  },
  sendTransaction: async function (txn) {
    const signer = await this.takeSigner();
    const response = await signer.sendTransaction(txn);
    this.releaseSigner(signer);
    return {response, signer_address: signer.address};
  },
  createRandomAccount: function () {
    return ethers.Wallet.createRandom().connect(ethers.provider);
  },
  takeSigner: async function () {
    if (!hre.parallelMode) {
      return this.getSignerForCurrentWorker();
    }

    if (this.signers.length == 0) {
      // Need to create new signers
      await this.initSigners();
    }

    return this.signers.pop();
  },
  releaseSigner: function (...signer) {
    this.signers.push(...signer);
  },
  getSignerForCurrentWorker: async function () {
    const signers = await ethers.getSigners();
    return signers[(process.env.MOCHA_WORKER_ID || 0) % signers.length];
  }
};

module.exports = parallelizer;
