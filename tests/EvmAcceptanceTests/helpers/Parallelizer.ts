import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import BN from "bn.js";
import hre, {ethers as hh_ethers, web3} from "hardhat";
import {initZilliqa, ScillaContract, Setup, UserDefinedLibrary} from "hardhat-scilla-plugin";
import {} from "hardhat-scilla-plugin/dist/src/index";

export type DeployOptions = {
  gasPrice?: string;
  gasLimit?: number;
  value?: BN;
};

export class Parallelizer {
  constructor() {
    const private_keys: string[] = hre.network["config"]["accounts"] as string[];

    this.zilliqaSetup = initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), private_keys, 30);
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

  zilliqaSetup: Setup;
}

export const parallelizer: Parallelizer = new Parallelizer();
