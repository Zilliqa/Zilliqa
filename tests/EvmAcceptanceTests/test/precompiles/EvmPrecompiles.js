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

  it("should return signer address when recover function is used", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const docHash = web3.utils.keccak256(msg);
    const privKey = general_helper.getPrivateAddressAt(0);
    const accountAddress = web3.eth.accounts.privateKeyToAccount(privKey).address;

    const signed = web3.eth.accounts.sign(docHash, privKey);
    const result = await contract.methods.testRecovery(docHash, signed.v, signed.r, signed.s).call({gasLimit: 7500000});
    expect(result).to.be.eq(accountAddress);
  });

  it("should return input value when identity function is used [@transactional]", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const hash = web3.utils.keccak256(msg);

    const sendResult = await contract.methods
      .testIdentity(hash)
      .send({gasLimit: 500000, from: web3_helper.getPrimaryAccountAddress()});
    expect(sendResult).to.be.not.null;
    const readValue = await contract.methods.idStored().call({gasLimit: 50000});
    expect(readValue).to.be.eq(hash);
  });

  it("should return correct hash when SHA2-256 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069";

    const readValue = await contract.methods.testSHA256(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("should return correct hash when Ripemd160 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x8476ee4631b9b30ac2754b0ee0c47e161d3f724c";

    const readValue = await contract.methods.testRipemd160(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("should return correct result when modexp function is used [@transactional]", async function () {
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

  it("should return correct result when ecAdd function is used", async function () {
    const result = await contract.methods.testEcAdd(1, 2, 1, 2).call();
    expect(web3.utils.toBN(result[0])).to.be.eq(
      web3.utils.toBN("030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd3")
    );
    expect(web3.utils.toBN(result[1])).to.be.eq(
      web3.utils.toBN("15ed738c0e0a7c92e7845f96b2ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4")
    );
  });

  it("should return correct result when ecMul function is used", async function () {
    const result = await contract.methods.testEcMul(1, 2, 2).call();
    expect(web3.utils.toBN(result[0])).to.be.eq(
      web3.utils.toBN("030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd3")
    );
    expect(web3.utils.toBN(result[1])).to.be.eq(
      web3.utils.toBN("15ed738c0e0a7c92e7845f96b2ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4")
    );
  });

  it("should return correct result when ecPairing function is used", async function () {
    const input = [
      "2cf44499d5d27bb186308b7af7af02ac5bc9eeb6a3d147c186b21fb1b76e18da",
      "2c0f001f52110ccfe69108924926e45f0b0c868df0e7bde1fe16d3242dc715f6",
      "1fb19bb476f6b9e44e2a32234da8212f61cd63919354bc06aef31e3cfaff3ebc",
      "22606845ff186793914e03e21df544c34ffe2f2f3504de8a79d9159eca2d98d9",
      "2bd368e28381e8eccb5fa81fc26cf3f048eea9abfdd85d7ed3ab3698d63e4f90",
      "2fe02e47887507adf0ff1743cbac6ba291e66f59be6bd763950bb16041a0a85e",
      "0000000000000000000000000000000000000000000000000000000000000001",
      "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd45",
      "1971ff0471b09fa93caaf13cbf443c1aede09cc4328f5a62aad45f40ec133eb4",
      "091058a3141822985733cbdddfed0fd8d6c104e9e9eff40bf5abfef9ab163bc7",
      "2a23af9a5ce2ba2796c1f4e453a370eb0af8c212d9dc9acd8fc02c2e907baea2",
      "23a8eb0b0996252cb548a4487da97b02422ebc0e834613f954de6c7e0afdc1fc"
    ].map((n) => web3.utils.toBN(n));
    const result = await contract.methods.testEcPairing(input).call();
    expect(web3.utils.toBN(result)).to.be.eq(1);
  });
});
