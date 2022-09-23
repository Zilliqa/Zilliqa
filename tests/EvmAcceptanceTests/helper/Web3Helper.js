const {web3} = require("hardhat");
const general_helper = require("./GeneralHelper");

var web3_helper = {
  primaryAccount: web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(0)),
  auxiliaryAccount: web3.eth.accounts.privateKeyToAccount(general_helper.getPrivateAddressAt(1)),

  deploy: async function (contractName, ...arguments) {
    const ContractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new web3.eth.Contract(ContractRaw.abi);
    return contract
      .deploy({data: ContractRaw.bytecode, arguments: arguments})
      .send({
        from: this.primaryAccount.address,
        ...(general_helper.isZilliqaNetworkSelected() && {gas: 35_000}),
        ...(general_helper.isZilliqaNetworkSelected() && {gasPrice: 4_000_000_000_000})
      })
      .on("error", function (error) {
        console.log(error);
      });
  }
};

module.exports = web3_helper;
