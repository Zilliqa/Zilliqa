const { web3 } = require("hardhat");
const general_helper = require('./GeneralHelper')


class Web3Helper {
    constructor() {
        this.primaryAccount = web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(0));
        this.auxiliaryAccount = web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(1));
    }

    getPrimaryAccount() {
        return this.primaryAccount;
    }

    async deploy(contractName, options = {}, ...args) {
        const contractRaw = hre.artifacts.readArtifactSync(contractName);
        const contract = new web3.eth.Contract(contractRaw.abi);
        const nonce = (options.nonce || await web3.eth.getTransactionCount(this.primaryAccount.address));
        const gasPrice = (options.gasPrice || await web3.eth.getGasPrice());
        const gasLimit = (options.gasLimit || 21_000);

        const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
            from: this.primaryAccount.address,
            nonce,
            gas: gasLimit,
            gasPrice: gasPrice,
            value: options.value ?? 0
        })

        return deployedContract;
    }

    async getCommonOptions(base = {}) {
        const gasPrice = (base.gasPrice || await web3.eth.getGasPrice());
        const gas = (base.gasLimit || 250000);
        const from = (base.account || this.primaryAccount.address);
        return {
            gasPrice, gas, from
        }
    }
}

module.exports = { Web3Helper}
