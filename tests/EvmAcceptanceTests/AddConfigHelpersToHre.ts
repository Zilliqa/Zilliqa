import {extendEnvironment} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types/runtime";
import SingerPool from "./helpers/parallel-tests/SignerPool";
import {Contract, Signer} from "ethers";
import {Contract as Web3Contract} from "web3-eth-contract";
import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {TransactionRequest, TransactionResponse} from "@ethersproject/providers";
import BN from "bn.js";

declare module "hardhat/types/runtime" {
  interface HardhatRuntimeEnvironment {
    signer_pool: SingerPool;
    debug: boolean;
    parallel: boolean;
    scillaTesting: boolean;
    isEthernalPluginEnabled: () => boolean;
    isScillaTestingEnabled: () => boolean;
    isZilliqaNetworkSelected: () => boolean;
    getEthChainId: () => number;
    getZilliqaChainId: () => number;
    getNetworkUrl: () => string;
    getWebsocketUrl: () => string;
    getWeb3ClientVersion: () => string;
    getProtocolVersion: () => number;
    getMiningState: () => boolean;
    getNetworkName: () => string;
    getASignerForContractDeployment: () => Promise<SignerWithAddress>;
    allocateSigner: () => SignerWithAddress;
    releaseSigner: (...signer: SignerWithAddress[]) => void;
    sendTransaction: (txn: TransactionRequest) => Promise<{response: TransactionResponse; signer_address: string}>;
    deployContract: (name: string, ...args: any[]) => Promise<Contract>;
    deployContractWithSigner: (name: string, signer: Signer, ...args: any[]) => Promise<Contract>;
    deployContractWeb3: (
      contractName: string,
      options: {gasPrice?: string; gasLimit?: number; value?: BN},
      ...args: any[]
    ) => Promise<Web3Contract>;
  }
}

extendEnvironment((hre: HardhatRuntimeEnvironment) => {
  hre.isScillaTestingEnabled = () => {
    return hre.scillaTesting;
  };

  hre.isZilliqaNetworkSelected = () => {
    return (hre as any).network.config.zilliqaNetwork;
  };

  hre.getEthChainId = () => {
    return (hre as any).network.config.chainId;
  };

  hre.getZilliqaChainId = () => {
    return (hre as any).network.config.chainId! - 0x8000;
  };

  hre.getNetworkUrl = () => {
    return (hre as any).network.config.url;
  };

  hre.getWebsocketUrl = () => {
    return (hre as any).network.config.websocketUrl;
  };

  hre.getWeb3ClientVersion = () => {
    return (hre as any).network.config.web3ClientVersion;
  };

  hre.getProtocolVersion = () => {
    return (hre as any).network.config.protocolVersion;
  };

  hre.getMiningState = () => {
    return (hre as any).network.config.miningState;
  };

  hre.getNetworkName = () => {
    return (hre as any).network.name;
  };

  hre.getASignerForContractDeployment = async (): Promise<SignerWithAddress> => {
    if (hre.parallel) {
      return hre.signer_pool.takeSigner();
    } else {
      return (await hre.ethers.getSigners())[0];
    }
  };

  /// If you call this function, consequently you must call `releaseSigner`, otherwise you'll run out of signers.
  hre.allocateSigner = (): SignerWithAddress => {
    return hre.signer_pool.takeSigner();
  };

  hre.releaseSigner = (...signer: SignerWithAddress[]) => {
    hre.signer_pool.releaseSigner(...signer);
  };

  hre.sendTransaction = async (txn: TransactionRequest) => {
    const signer = hre.allocateSigner();
    const response = await signer.sendTransaction(txn);
    hre.releaseSigner(signer);

    return {response, signer_address: await signer.getAddress()};
  };

  hre.deployContract = async (name: string, ...args: any[]): Promise<Contract> => {
    const signer = await hre.getASignerForContractDeployment();
    return hre.deployContractWithSigner(name, signer, ...args);
  };

  hre.deployContractWithSigner = async (name: string, signer: Signer, ...args: any[]): Promise<Contract> => {
    const factory = await hre.ethers.getContractFactory(name);
    return (await factory.connect(signer).deploy(...args)).deployed();
  };

  // TODO: remove any type from `options`
  hre.deployContractWeb3 = async (
    contractName: string,
    options: {gasPrice?: string; gasLimit?: number; value?: BN},
    ...args: any[]
  ): Promise<Web3Contract> => {
    const signer = await hre.getASignerForContractDeployment();

    const contractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new hre.web3.eth.Contract(contractRaw.abi);
    const gasPrice = options.gasPrice || (await hre.web3.eth.getGasPrice());
    const gasLimit = options.gasLimit || 210_000;
    const value = options.value || 0;

    const signerAddress = await signer.getAddress();

    const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
      from: signerAddress,
      gas: gasLimit,
      gasPrice,
      value
    });

    deployedContract.options.from = signerAddress;
    deployedContract.options.gas = gasLimit;
    return deployedContract;
  };
});
