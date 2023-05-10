import {TransactionRequest} from "@ethersproject/providers";
import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import BN from "bn.js";
import {Signer, Wallet} from "ethers";
import hre, {ethers as hh_ethers, web3} from "hardhat";
import {initZilliqa, ScillaContract, Setup, UserDefinedLibrary} from "hardhat-scilla-plugin";
import SignerPool from "./SignerPool";

export type DeployOptions = {
  gasPrice?: string;
  gasLimit?: number;
  value?: BN;
};

export class Parallelizer {
  constructor() {
    const privateKey = "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004";
    this.zilliqaAccountAddress = getAddressFromPrivateKey(privateKey);
    this.zilliqaSetup = initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), [privateKey], 30);
  }

  async deployContract(contractName: string, ...args: any[]) {
    let signer: Signer;

    if (hre.parallel) {
      signer = await this.signerPool.takeSigner();
    } else {
      signer = await SignerPool.getSignerForCurrentWorker();
    }

    const Contract = await hh_ethers.getContractFactory(contractName);
    const deployedContract = await Contract.connect(signer).deploy(...args);
    if (hre.isEthernalPluginEnabled()) {
      hre.ethernal.push({name: contractName, address: deployedContract.address});
    }
    return deployedContract;
  }

  async deployContractWithSigner(signer: Signer, contractName: string, ...args: any[]) {
    const Contract = await hh_ethers.getContractFactory(contractName);
    const deployedContract = await Contract.connect(signer).deploy(...args);
    if (hre.isEthernalPluginEnabled()) {
      hre.ethernal.push({name: contractName, address: deployedContract.address});
    }
    return deployedContract;
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
    if (hre.isEthernalPluginEnabled()) {
      hre.ethernal.push({name: contractName, address: deployedContract.address});
    }
    return deployedContract;
  }

  async deployScillaContract(contractName: string, ...args: any[]): Promise<ScillaContract> {
    return hre.deployScillaContract(contractName, ...args);
  }

  async deployScillaLibrary(libraryName: string): Promise<ScillaContract> {
    return hre.deployScillaLibrary(libraryName);
  }

  async deployScillaContractWithLibrary(
    libraryName: string,
    userDefinedLibraries: UserDefinedLibrary[],
    ...args: any[]
  ): Promise<ScillaContract> {
    return hre.deployScillaContractWithLib(libraryName, userDefinedLibraries, ...args);
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

  zilliqaAccountAddress: string;
  zilliqaSetup: Setup;
  private signerPool: SignerPool = new SignerPool();
}

export const parallelizer: Parallelizer = new Parallelizer();
