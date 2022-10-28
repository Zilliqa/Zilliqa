const {web3} = require("hardhat");
const general_helper = require("./GeneralHelper");

var web3_helper = {
  primaryAccount: web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(0)),

  secondaryAccount: web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(1)),

  getPrimaryAccountAddress: function () {
    return this.primaryAccount.address;
  },

  getSecondaryAccountAddress: function () {
    return this.secondaryAccount.address;
  },

  deploy: async function (contractName, options = {}, ...args) {
    const contractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new web3.eth.Contract(contractRaw.abi);
    const nonce = options.nonce || (await web3.eth.getTransactionCount(this.getPrimaryAccountAddress()));
    const gasPrice = options.gasPrice || (await web3.eth.getGasPrice());
    const gasLimit = options.gasLimit || 210_000;

    const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
      from: this.getPrimaryAccountAddress(),
      nonce,
      gas: gasLimit,
      gasPrice: gasPrice,
      value: options.value ?? 0
    });

    return deployedContract;
  },

  getCommonOptions: async function (base = {}) {
    const gasPrice = base.gasPrice || (await web3.eth.getGasPrice());
    const gas = base.gasLimit || 250000;
    const from = base.account || this.getPrimaryAccountAddress();
    return {
      gasPrice,
      gas,
      from
    };
  }
};

module.exports = web3_helper;
