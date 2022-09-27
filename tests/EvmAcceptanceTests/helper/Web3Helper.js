const { web3 } = require("hardhat");
const general_helper = require('./GeneralHelper')


getPrimaryAccount = function() {
    return web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(0));
}

var web3_helper = {

    getPrimaryAccountAddress: function() {
        return getPrimaryAccount().address;
    },
    
    deploy: async function(contractName, options = {}, ...args) {
        const contractRaw = hre.artifacts.readArtifactSync(contractName);
        const contract = new web3.eth.Contract(contractRaw.abi);
        const nonce = (options.nonce || await web3.eth.getTransactionCount(getPrimaryAccount().address));
        const gasPrice = (options.gasPrice || await web3.eth.getGasPrice());
        const gasLimit = (options.gasLimit || 21_000);

        const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
            from: getPrimaryAccount().address,
            nonce,
            gas: gasLimit,
            gasPrice: gasPrice,
            value: options.value ?? 0
        })

        return deployedContract;
    },

    getCommonOptions: async function(base = {}) {
        const gasPrice = (base.gasPrice || await web3.eth.getGasPrice());
        const gas = (base.gasLimit || 250000);
        const from = (base.account || getPrimaryAccount().address);
        return {
            gasPrice, gas, from
        }
    }
}

module.exports = web3_helper
