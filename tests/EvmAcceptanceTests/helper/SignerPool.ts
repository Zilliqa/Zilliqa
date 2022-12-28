import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {Wallet} from "ethers";
import {ethers} from "hardhat";

export default class SignerPool {
  public static async getSignerForCurrentWorker(): Promise<SignerWithAddress> {
    const signers = await ethers.getSigners();
    let current_worker_id = 0;
    if (process.env.MOCHA_WORKER_ID) {
      current_worker_id = Number(process.env.MOCHA_WORKER_ID);
    }
    return signers[current_worker_id % signers.length];
  }

  public static createRandomAccount(): Wallet {
    return Wallet.createRandom().connect(ethers.provider);
  }

  public async takeSigner(): Promise<Wallet> {
    if (this.signers.length == 0) {
      // Need to create new signers
      await this.initSigners();
    }

    return this.signers.pop()!;
  }

  public releaseSigner(...signer: Wallet[]) {
    this.signers.push(...signer);
  }

  private async initSigners() {
    const signer = await SignerPool.getSignerForCurrentWorker();
    const newSigners = Array.from({length: 10}, (v, k) => Wallet.createRandom().connect(ethers.provider));
    const BatchTransferContract = await ethers.getContractFactory("BatchTransfer");
    const batchTransfer = await BatchTransferContract.connect(signer).deploy({value: 10_000_000_000_000});
    await batchTransfer.deployed();
    const addresses = newSigners.map((signer) => signer.address);
    await batchTransfer.batchTransfer(addresses, 1_000_000_000_000);

    this.signers.push(...newSigners);
  }

  private signers: Wallet[] = [];
}
