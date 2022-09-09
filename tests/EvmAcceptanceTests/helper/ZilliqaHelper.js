const { ethers, web3 } = require("hardhat")

var zilliqa = {
    primaryAccount: web3.eth.accounts.privateKeyToAccount(hre.getPrivateAddressAt(0)),
    getState: async function(address, index) {
        return web3.eth.getStorageAt(address, index)
    },
    
    getStateAsNumber: async function(address, index) {
        const state = await web3.eth.getStorageAt(address, index)
        return web3.utils.hexToNumber(state)
    },
    
    getStateAsString: async function(address, index) {
        const state = await web3.eth.getStorageAt(address, index)
        return web3.utils.hexToUtf8(state.slice(0, -2))
    },

    deployContract: async function(contractName, options = {}) {
        const Contract = await ethers.getContractFactory(contractName);
    
        const senderAccount = (options.senderAccount || this.primaryAccount)
        const constructorArgs = (options.constructorArgs || []);
    
        const nonce = await web3.eth.getTransactionCount(senderAccount.address, 'latest'); // nonce starts counting from 0

        const transaction = {
            'from': senderAccount.address,
            'value': options.value ?? 0,
            'data': Contract.getDeployTransaction(...constructorArgs).data,
            'gas': 300000,
            'gasPrice': 2000000000,
            'chainId': hre.getEthChainId(),
            'nonce': nonce,
        };
    
        const receipt = await this.sendTransaction(transaction, senderAccount)
    
        const contract = new web3.eth.Contract(hre.artifacts.readArtifactSync(contractName).abi, receipt.contractAddress, {
                "from": this.primaryAccount.address
            })
    
        return contract
    },

    callContract: async function(contract, func_name, ...params) {
        return this.callContractBy(this.primaryAccount, contract, func_name, ...params)
    },

    callContractBy: async function(senderAccount, contract, func_name, ...params) {
        const abi = contract.methods[func_name](...params).encodeABI()
        const nonce = await web3.eth.getTransactionCount(senderAccount.address, 'latest'); // nonce starts counting from 0

        const transaction = {
            'to': contract._address,
            'from': senderAccount.address,
            'value': 0,
            'data': abi,
            'gas': 300000,
            'gasPrice': 2000000000,
            'chainId': hre.getEthChainId(),
            'nonce': nonce,
        };
        
        const receipt = await this.sendTransaction(transaction, this.primaryAccount)
        await web3.eth.getTransaction(receipt.transactionHash)
    },

    callView: async function(contract, func_name, ...params) {
        return this.callViewBy(this.primaryAccount, contract, func_name, ...params)
    },

    callViewBy: async function(senderAccount, contract, func_name, ...params) {
        const abi = contract.methods[func_name](...params).encodeABI()

        const transaction = {
            from: senderAccount.address,
            to: contract._address,
            data: abi,
            gasPrice: 2000000000000000,
        };

        return web3.eth.call(transaction)
    },

    sendTransaction: async function(tx, senderAccount) {
        const signedTx = await senderAccount.signTransaction(tx);
        return web3.eth.sendSignedTransaction(signedTx.rawTransaction)
    },

    moveFundsBy: async function(amount, toAddr, senderAccount) {
        try {
            const nonce = await web3.eth.getTransactionCount(senderAccount.address); // nonce starts counting from 0
            const tx = {
                'to': toAddr,
                'value': amount,
                'gas': 300000,
                'gasPrice': 2000000000000000,
                'nonce': nonce,
                'chainId': hre.getEthChainId(),
                'data': ""
            }

            return this.sendTransaction(tx, senderAccount)
        } catch (err) {
            console.log("theres an error...");
            console.log(err);
        }
    },

    moveFunds: async function(amount, toAddr) {
        return this.moveFundsBy(amount, toAddr, this.primaryAccount)
    }

}

module.exports = zilliqa