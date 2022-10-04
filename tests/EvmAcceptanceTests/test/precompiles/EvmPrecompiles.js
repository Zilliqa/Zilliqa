const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../../helper/Web3Helper");
const general_helper = require("../../helper/GeneralHelper");

describe("Precompile tests with web3.js", function () {
  describe("When recover function is used", function () {
    let contract;
    let options;
    before(async function () {
      options = await web3_helper.getCommonOptions();
      contract = await web3_helper.deploy("Precompiles", options);
    });

    it("Should return signer address", async function () {
      const msg = web3.utils.toHex("SomeMessage");
      const docHash = web3.utils.keccak256(msg);
      const privKey = general_helper.getPrivateAddressAt(0);
      const accountAddress = web3.eth.accounts.privateKeyToAccount(privKey).address;

      const signed = web3.eth.accounts.sign(docHash, privKey);
      const result = await contract.methods
        .testRecovery(docHash, signed.v, signed.r, signed.s)
        .call({gasLimit: 4500000});
      expect(result).to.be.eq(accountAddress);
    });
  });
});
