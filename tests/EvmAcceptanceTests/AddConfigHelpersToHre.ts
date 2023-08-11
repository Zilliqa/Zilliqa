import {extendEnvironment} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types/runtime";
import SingerPool from "./helpers/parallel-tests/SignerPool";
import {Contract, Signer} from "ethers";
import { Contract as Web3Contract } from "web3-eth-contract";

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
    deployContract: (name: string) => Promise<Contract>;
    deployContractWithSigner: (name: string, signer: Signer) => Promise<Contract>;
    deployContractWeb3: (contractName: string, ...args: any[]) => Promise<Web3Contract>;
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

  hre.deployContract = async (name: string): Promise<Contract> => {
    const signer = hre.signer_pool.takeSigner();
    return hre.deployContractWithSigner(name, signer);
  };

  hre.deployContractWithSigner = async (name: string, signer: Signer): Promise<Contract> => {
    const factory = await hre.ethers.getContractFactory(name);
    return factory.connect(signer).deploy();
  };

  hre.deployContractWeb3 = async (contractName: string, ...args: any[]): Promise<Web3Contract> => {
    const signer = hre.signer_pool.takeSigner();

    const contractRaw = hre.artifacts.readArtifactSync(contractName);
    const contract = new hre.web3.eth.Contract(contractRaw.abi);
    const gasPrice = await hre.web3.eth.getGasPrice();
    const gasLimit = 210_000;
    const signerAddress = await signer.getAddress();

    const deployedContract = await contract.deploy({data: contractRaw.bytecode, arguments: args}).send({
      from: signerAddress,
      gas: gasLimit,
      gasPrice: gasPrice,
      value: 0
    });

    deployedContract.options.from = signerAddress;
    deployedContract.options.gas = gasLimit;
    return deployedContract;
  };
});
