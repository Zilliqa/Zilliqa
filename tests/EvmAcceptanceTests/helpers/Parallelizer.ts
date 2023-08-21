import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import BN from "bn.js";
import hre, {ethers as hh_ethers, web3} from "hardhat";
import {initZilliqa, ScillaContract, Setup, UserDefinedLibrary} from "hardhat-scilla-plugin";
import {} from "hardhat-scilla-plugin/dist/src/index"

export type DeployOptions = {
  gasPrice?: string;
  gasLimit?: number;
  value?: BN;
};

export class Parallelizer {
  constructor() {
    let privateKey = "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba";
    if (process.env.PRIMARY_ACCOUNT !== undefined) {
      privateKey = process.env.PRIMARY_ACCOUNT;
    }

    const private_keys: string[] = hre.network["config"]["accounts"] as string[];

    this.zilliqaAccountAddress = getAddressFromPrivateKey(privateKey);
    this.zilliqaSetup = initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), [privateKey, ...private_keys], 30);
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

  zilliqaAccountAddress: string;
  zilliqaSetup: Setup;
}

export const parallelizer: Parallelizer = new Parallelizer();
