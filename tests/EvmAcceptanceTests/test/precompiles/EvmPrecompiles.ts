import {expect} from "chai";
import {parallelizer} from "../../helpers";
import {web3} from "hardhat";

describe("Precompile tests with web3.js", function () {
  before(async function () {
    this.contract = await parallelizer.deployContractWeb3("Precompiles");
  });

  it("should return signer address when recover function is used", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const docHash = web3.utils.keccak256(msg);
    const account = web3.eth.accounts.create();
    const accountAddress = account.address;
    const signed = account.sign(docHash);
    const result = await this.contract.methods
      .testRecovery(docHash, signed.v, signed.r, signed.s)
      .call({gasLimit: 7500000});

    expect(result).to.be.eq(accountAddress);
  });

  it("should return input value when identity function is used [@transactional]", async function () {
    const msg = web3.utils.toHex("SomeMessage");
    const hash = web3.utils.keccak256(msg);

    const sendResult = await this.contract.methods.testIdentity(hash).send();
    expect(sendResult).to.be.not.null;
    const readValue = await this.contract.methods.idStored().call({gasLimit: 50000});
    expect(readValue).to.be.eq(hash);
  });

  it("should return correct hash when SHA2-256 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069";

    const readValue = await this.contract.methods.testSHA256(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("should return correct hash when Ripemd160 function is used", async function () {
    const msg = "Hello World!";
    const expectedHash = "0x8476ee4631b9b30ac2754b0ee0c47e161d3f724c";

    const readValue = await this.contract.methods.testRipemd160(msg).call({gasLimit: 50000});
    expect(readValue).to.be.eq(expectedHash);
  });

  it("should return correct result when modexp function is used [@transactional]", async function () {
    const base = 8;
    const exponent = 9;
    const modulus = 10;
    const expectedResult = 8;

    const sendResult = await this.contract.methods.testModexp(base, exponent, modulus).send();
    expect(sendResult).to.be.not.null;

    const readValue = await this.contract.methods.modExpResult().call({gasLimit: 50000});
    expect(web3.utils.toBN(readValue)).to.be.eq(web3.utils.toBN(expectedResult));
  });

  it("should return correct result when ecAdd function is used", async function () {
    const result = await this.contract.methods.testEcAdd(1, 2, 1, 2).call();
    expect(web3.utils.toBN(result[0])).to.be.eq(
      web3.utils.toBN("030644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd3")
    );
    expect(web3.utils.toBN(result[1])).to.be.eq(
      web3.utils.toBN("15ed738c0e0a7c92e7845f96b2ae9c0a68a6a449e3538fc7ff3ebf7a5a18a2c4")
    );
  });

  it("should return correct result when ecMul function is used", async function () {
    const result = await this.contract.methods.testEcMul(1, 2, 2).call();
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
    const result = await this.contract.methods.testEcPairing(input).call();
    expect(web3.utils.toBN(result)).to.be.eq(1);
  });

  it("should return correct result when blake2 function is used", async function () {
    const ROUNDS = 12;
    const H = [
      "0x48c9bdf267e6096a3ba7ca8485ae67bb2bf894fe72f36e3cf1361d5f3af54fa5",
      "0xd182e6ad7f520e511f6c3e2b8c68059b6bbd41fbabd9831f79217e1319cde05b"
    ];
    const M = [
      "0x6162630000000000000000000000000000000000000000000000000000000000",
      "0x0000000000000000000000000000000000000000000000000000000000000000",
      "0x0000000000000000000000000000000000000000000000000000000000000000",
      "0x0000000000000000000000000000000000000000000000000000000000000000"
    ];
    const T = ["0x0300000000000000", "0x0000000000000000"];
    const F = true;

    const EXPECTED = [
      "0xba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1",
      "0x7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923"
    ];

    expect(await this.contract.methods.testBlake2(ROUNDS, H, M, T, F).call()).to.be.deep.eq(EXPECTED);
  });
});
