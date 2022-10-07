const {expect} = require("chai");
const {web3} = require("hardhat");
const web3_helper = require("../../helper/Web3Helper");
const general_helper = require("../../helper/GeneralHelper");

describe("Precompile tests with web3.js", function () {
  let contract;
  let options;
  before(async function () {
    options = await web3_helper.getCommonOptions();
    contract = await web3_helper.deploy("Precompiles", options);
  });

  it("Should return signer address when recover function is used", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const docHash = web3.utils.keccak256(msg);
    const privKey = general_helper.getPrivateAddressAt(0);
    const accountAddress = web3.eth.accounts.privateKeyToAccount(privKey).address;

    const signed = web3.eth.accounts.sign(docHash, privKey);
    const result = await contract.methods.testRecovery(docHash, signed.v, signed.r, signed.s).call({gasLimit: 7500000});
    expect(result).to.be.eq(accountAddress);
  });

  it("Should return input value when identity function is used", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const hash = web3.utils.keccak256(msg);

    const sendResult = await contract.methods
      .testIdentity(hash)
      .send({gasLimit: 500000, from: web3_helper.getPrimaryAccountAddress()});
    expect(sendResult).to.be.not.null;
    const readValue = await contract.methods.idStored().call({gasLimit: 50000});
    expect(readValue).to.be.eq(hash);
  });

  it("Should return correct hash when SHA2-256 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069";

    const readValue = await contract.methods.testSHA256(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("Should return correct hash when Ripemd160 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x8476ee4631b9b30ac2754b0ee0c47e161d3f724c";

    const readValue = await contract.methods.testRipemd160(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("Should return correct result when modexp function is used", async function () {
    const base = 8;
    const exponent = 9;
    const modulus = 10;
    const expectedResult = 8;

    const sendResult = await contract.methods
      .testModexp(base, exponent, modulus)
      .send({gasLimit: 700000, from: web3_helper.getPrimaryAccountAddress()});
    expect(sendResult).to.be.not.null;

    const readValue = await contract.methods.modExpResult().call({gasLimit: 50000});
    expect(web3.utils.toBN(readValue)).to.be.eq(web3.utils.toBN(expectedResult));
  });
});
