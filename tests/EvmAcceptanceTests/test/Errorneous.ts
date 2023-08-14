import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";

// FIXME: It takes around 13 seconds to finish on iso server, is it ok?!
describe("While Calling a method on erroneous contract with given gas limit #parallel", function () {
  let contract: Contract;
  let signer: SignerWithAddress;
  before(async function () {
    signer = hre.signer_pool.takeSigner();
    contract = await hre.deployContractWithSigner("Erroneous", signer);
  });

  it("it should return to the client and nonce/balance should be affected @block-1", async function () {
    const funds = await signer.getBalance();
    const nonce = await signer.getTransactionCount();
    await contract.foo({gasLimit: 5000000});
    expect(funds).to.be.greaterThan(await signer.getBalance());
    expect(nonce).to.be.lessThan(await signer.getTransactionCount());
  });
});
