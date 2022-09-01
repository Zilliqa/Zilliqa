const { Zilliqa } = require('@zilliqa-js/zilliqa')
const {schnorr} = require('@zilliqa-js/crypto');
const { Account } = require('@zilliqa-js/account')
const { ethers, web3} = require("hardhat")
const hre = require("hardhat")
const { getPubKeyFromPrivateKey, getAddressFromPrivateKey, toChecksumAddress } = require('@zilliqa-js/crypto');
const { BN, Long, bytes, units } = require('@zilliqa-js/util');
const axios = require('axios')

class ZilliqaHelper {
    constructor() {
        this.zilliqa = new Zilliqa(this.getNetworkUrl());
        this.primaryAccount = web3.eth.accounts.privateKeyToAccount(this.getPrimaryPrivateAddress())
        this.auxiliaryAccount = web3.eth.accounts.privateKeyToAccount(this.getPrivateAddress(1))

        // TODO: Remove zilliqa dependency
        this.zilliqa.wallet.addByPrivateKey(this.getPrimaryPrivateAddress());
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

    getVersion() {
        const MSG_VERSION = 1;
        return bytes.pack(this.getZilliqaChainId(), MSG_VERSION);
    }

    getEthChainId() {
        return hre.network.config.chainId;
    }

    getZilliqaChainId() {
        return hre.network.config.chainId - 0x8000;
    }

    getNetworkUrl() {
        return hre.network.config.url
    }

    getPrimaryPrivateAddress() {
        return this.getPrivateAddress(0)
    }

    getAuxiliaryAccount() {
        return this.auxiliaryAccount
    }

    getPrivateAddress(index) {
        return hre.network.config.accounts[index]
    }

    async deployContract(contractName, options = {}) {
        const Contract = await ethers.getContractFactory(contractName);

        const senderAccount = (options.senderAccount || this.auxiliaryAccount)
        const constructorArgs = (options.constructorArgs || []);
    
        // Give our Eth address some monies
        await this.moveFunds('100000', toChecksumAddress(senderAccount.address), this.primaryAccount)

        // Deploy a SC using web3 API ONLY
        const nonce = await web3.eth.getTransactionCount(this.auxiliaryAccount.address, 'latest'); // nonce starts counting from 0
    
        const transaction = {
            'from': senderAccount.address,
            'value': options.value ?? 0,
            'data': Contract.getDeployTransaction(...constructorArgs).data,
            'gas': 300000,
            'gasPrice': 2000000000,
            'chainId': this.getEthChainId(),
            'nonce': nonce,
        };

        const receipt = await this.sendTransaction(transaction, senderAccount)

        const contractAddress = await this.zilliqa.blockchain.getContractAddressFromTransactionID(receipt.transactionHash.replace("0x", ""));
        const contract = new web3.eth.Contract(hre.artifacts.readArtifactSync(contractName).abi,
            web3.utils.toChecksumAddress((contractAddress).result), {
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
            'gasPrice': 2000000000,
            'chainId': this.getEthChainId(),
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
            gasPrice: 2000000000,
        };

        return web3.eth.call(transaction)
    }

    async sendTransaction(tx, senderAccount) {
        const signedTx = await senderAccount.signTransaction(tx);
        return web3.eth.sendSignedTransaction(signedTx.rawTransaction)
    }

    async moveFunds(amount, toAddr) {
        try {
            const myGasPrice = units.toQa('2000', units.Units.Li); // Gas Price that will be used by all transactions

            const tx_to_send = this.zilliqa.transactions.new(
                {
                    version: this.getVersion(),
                    toAddr: toAddr,
                    amount: new BN(units.toQa(amount, units.Units.Zil)),
                    gasPrice: myGasPrice,
                    gasLimit: Long.fromNumber(50),
                    chainId: this.getZilliqaChainId(),
                },
                false,
            );

            const tx = await this.zilliqa.blockchain.createTransactionWithoutConfirm(tx_to_send);

            const confirmedTxn = await tx.confirm(tx.id);
        } catch (err) {
            console.log("theres an error...");
            console.log(err);
        }
    }

    async callEthMethod(method, id, params, callback) {
        const data = {
            id: id,
            jsonrpc: "2.0",
            method: method,
            params: params
        }
    
        const host = this.getNetworkUrl()

        // ASYNC
        if(typeof callback === 'function') {
            await axios.post(host, data).then(response => {
                if(response.status === 200) {
                    callback(response.data, response.status);
                } else {
                    throw new Error('Can\'t connect to '+ host + "\n Send: "+ JSON.stringify(data, null, 2));
                }
            })
        // SYNC
        } else {
            const response = await axios.post(host, data)

            if(response.status !== 200) {
                throw new Error('Can\'t connect to '+ host + "\n Send: "+ JSON.stringify(data, null, 2));
            }

            return response.data
        }
    }
}

module.exports = {
    ZilliqaHelper,
};