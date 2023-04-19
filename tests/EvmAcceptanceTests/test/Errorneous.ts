import {expect} from "chai";
import {parallelizer} from "../helpers";
import {Signer} from "ethers";

describe("While Calling a method on erroneous contract with given gas limit", function () {
  let signer: Signer;
  before(async function () {
    signer = await parallelizer.takeSigner();
    this.contract = await parallelizer.deployContractWithSigner(signer, "Erroneous");
  });

  it("it should return to the client and nonce/balance should be affected", async function () {
    const funds = await signer.getBalance();
    const nonce = await signer.getTransactionCount();
    await this.contract.connect(signer).foo({gasLimit: 5000000});
    expect(funds).to.be.greaterThan(await signer.getBalance());
    expect(nonce).to.be.lessThan(await signer.getTransactionCount());
  });
});
