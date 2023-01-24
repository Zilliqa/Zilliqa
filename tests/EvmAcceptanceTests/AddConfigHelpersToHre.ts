import {extendEnvironment} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types/runtime";

declare module "hardhat/types/runtime" {
  interface HardhatRuntimeEnvironment {
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
  }
}

extendEnvironment((hre: HardhatRuntimeEnvironment) => {
  hre.isEthernalPluginEnabled = () => {
    return !hre.config.ethernal.disabled;
  };

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
});
