import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import BN from "bn.js";
import hre, {ethers as hh_ethers, web3} from "hardhat";
import {initZilliqa, ScillaContract, Setup, UserDefinedLibrary} from "hardhat-scilla-plugin";

export type DeployOptions = {
  gasPrice?: string;
  gasLimit?: number;
  value?: BN;
};

export class Parallelizer {
  constructor() {
    let privateKey = "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004";
    if (process.env.PRIMARY_ACCOUNT !== undefined) {
      privateKey = process.env.PRIMARY_ACCOUNT;
    }

    this.zilliqaAccountAddress = getAddressFromPrivateKey(privateKey);
    this.zilliqaSetup = initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), [privateKey], 30);
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
