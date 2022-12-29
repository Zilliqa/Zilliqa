import {TransactionRequest} from "@ethersproject/providers";
import {Signer, Wallet} from "ethers";
import hre, {ethers as hh_ethers, web3} from "hardhat";
import SignerPool from "./SignerPool";
import BN from "bn.js";

export type DeployOptions = {
  gasPrice?: string;
  gasLimit?: number;
  value?: BN;
};

export class Parallelizer {
  async deployContract(contractName: string, ...args: any[]) {
    let signer: Signer;

    if (hre.parallel) {
      signer = await this.signerPool.takeSigner();
    } else {
      signer = await SignerPool.getSignerForCurrentWorker();
    }

    web3.eth.getAccounts();
    const Contract = await hh_ethers.getContractFactory(contractName);
    return Contract.connect(signer).deploy(...args);
  }

  async deployContractWeb3(contractName: string, options: DeployOptions = {}, ...args: any[]) {
    const signer = await this.signerPool.takeSigner();

    web3.eth.accounts.wallet.add(signer.privateKey);
    const contractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new web3.eth.Contract(contractRaw.abi);
    const gasPrice = options.gasPrice || (await web3.eth.getGasPrice());
    const gasLimit = options.gasLimit || 210_000;
    const signerAddress = await signer.getAddress();

    const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
      from: signerAddress,
      gas: gasLimit,
      gasPrice: gasPrice,
      value: options.value ?? 0
    });

    deployedContract.options.from = signerAddress;
    deployedContract.options.gas = gasLimit;
    return deployedContract;
  }

  async sendTransaction(txn: TransactionRequest) {
    const signer = await this.signerPool.takeSigner();
    const response = await signer.sendTransaction(txn);
    this.signerPool.releaseSigner(signer);
    return {response, signer_address: await signer.getAddress()};
  }

  async takeSigner(): Promise<Wallet> {
    return this.signerPool.takeSigner();
  }

  public releaseSigner(...signer: Wallet[]) {
    this.signerPool.releaseSigner(...signer);
  }

  private signerPool: SignerPool = new SignerPool();
}

const parallelizer: Parallelizer = new Parallelizer();
export default parallelizer;
