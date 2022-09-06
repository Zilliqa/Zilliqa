const { web3 } = require("hardhat");

var web3_helper = {
    primaryAccount: web3.eth.accounts.privateKeyToAccount(hre.getPrivateAddressAt(0)),
    auxiliaryAccount: web3.eth.accounts.privateKeyToAccount(hre.getPrivateAddressAt(1)),

    deploy: async function(contractName, ...arguments) {
        const ContractRaw = hre.artifacts.readArtifactSync(contractName)
        const contract = new web3.eth.Contract(ContractRaw.abi, {
            from: this.primaryAccount.address
        })
        return contract.deploy({ data: ContractRaw.bytecode, arguments: arguments })
            .send({
                from: this.primaryAccount.address,
                ...(hre.isZilliqaNetworkSelected()) && { gas: 30_000 },
                ...(hre.isZilliqaNetworkSelected()) && { gasPrice: 2_000_000_000 },
            })
            .on('error', function (error) { console.log(error) })
    }
}

module.exports = web3_helper