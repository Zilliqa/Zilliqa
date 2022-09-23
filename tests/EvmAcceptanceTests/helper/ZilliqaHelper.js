const { ethers, web3 } = require("hardhat")
const hre = require("hardhat")
const general_helper = require('../helper/GeneralHelper')

class ZilliqaHelper {
    constructor() {
        this.primaryAccount = web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(0))
        this.auxiliaryAccount = web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(1))
    }

    getPrimaryAccount() {
        return this.primaryAccount;
    }

    getSecondaryAccount() {
        return this.primaryAccount;
    }

    async getState(address, index) {
        return web3.eth.getStorageAt(address, index)
    }

    async getStateAsNumber(address, index) {
        const state = await web3.eth.getStorageAt(address, index)
        return web3.utils.hexToNumber(state)
    }

    async getStateAsString(address, index) {
        const state = await web3.eth.getStorageAt(address, index)
        return web3.utils.hexToUtf8(state.slice(0, -2))
    }

    async deployContract(contractName, options = {}) {
        const Contract = await ethers.getContractFactory(contractName);

        const senderAccount = (options.senderAccount || this.auxiliaryAccount)
        const constructorArgs = (options.constructorArgs || []);
        const gasLimit = (options.gasLimit || 250000);
        const gasPrice = (options.gasPrice || await web3.eth.getGasPrice());

        // Deploy a SC using web3 API ONLY
        const nonce = await web3.eth.getTransactionCount(senderAccount.address, 'latest'); // nonce starts counting from 0

        const transaction = {
            'from': senderAccount.address,
            'value': options.value ?? 0,
            'data': Contract.getDeployTransaction(...constructorArgs).data,
            'gas': gasLimit,
            'gasPrice': gasPrice,
            'chainId': general_helper.getEthChainId(),
            'nonce': nonce,
        };

        const receipt = await this.sendTransaction(transaction, senderAccount)

        const contract = new web3.eth.Contract(hre.artifacts.readArtifactSync(contractName).abi, receipt.contractAddress, {
            "from": this.auxiliaryAccount.address
        })

        return contract
    }

    async callContract(contract, func_name, ...params) {
        return this.callContractBy(this.auxiliaryAccount, contract, func_name, ...params)
    }

    async callContractBy(senderAccount, contract, func_name, ...params) {
        const abi = contract.methods[func_name](...params).encodeABI()
        const nonce = await web3.eth.getTransactionCount(senderAccount.address, 'latest'); // nonce starts counting from 0

        const transaction = {
            'to': contract._address,
            'from': senderAccount.address,
            'value': 0,
            'data': abi,
            'gas': 300000,
            'gasPrice': 2000000000000000,
            'chainId': general_helper.getEthChainId(),
            'nonce': nonce,
        };

        const receipt = await this.sendTransaction(transaction, this.auxiliaryAccount)
        await web3.eth.getTransaction(receipt.transactionHash)
    }

    async callView(contract, func_name, ...params) {
        return this.callViewBy(this.auxiliaryAccount, contract, func_name, ...params)
    }

    async callViewBy(senderAccount, contract, func_name, ...params) {
        const abi = contract.methods[func_name](...params).encodeABI()

        const transaction = {
            from: senderAccount.address,
            to: contract._address,
            data: abi,
            gasPrice: 2000000000000000,
        };

        return web3.eth.call(transaction)
    }

    async sendTransaction(tx, senderAccount) {
        const signedTx = await senderAccount.signTransaction(tx);

        console.log("Send transaction from sender:", senderAccount.address, " to:", tx.to);

        return web3.eth.sendSignedTransaction(signedTx.rawTransaction)
    }

    async moveFundsBy(amount, toAddr, senderAccount) {
        try {
            const nonce = await web3.eth.getTransactionCount(senderAccount.address); // nonce starts counting from 0
            const tx = {
                'to': toAddr,
                'value': amount,
                'gas': 21_000,
                'gasPrice': await web3.eth.getGasPrice(),
                'nonce': nonce,
                'chainId': general_helper.getEthChainId(),
                'data': ""
            }

            return this.sendTransaction(tx, senderAccount)
        } catch (err) {
            console.log("theres an error...", err);
        }
    }

    async moveFunds(amount, toAddr) {
        return this.moveFundsBy(amount, toAddr, this.primaryAccount)
    }
}

module.exports = {
    ZilliqaHelper,
};
