import {BN, Zilliqa, getAddressFromPrivateKey, units} from "@zilliqa-js/zilliqa";
import {BigNumber} from "ethers";
import {ethers} from "ethers";
import {HardhatRuntimeEnvironment} from "hardhat/types";

/// HRE CAN'T BE IMPORTED, IT SHOULD BE PASSED AS AN ARGUMENT.

export const getEthBalance = async (
  hre: HardhatRuntimeEnvironment,
  privateKey: string
): Promise<[address: string, balance: BigNumber]> => {
  const wallet = new ethers.Wallet(privateKey, hre.ethers.provider);
  return [wallet.address, await wallet.getBalance()];
};

export const getZilBalance = async (
  hre: HardhatRuntimeEnvironment,
  privateKey: string
): Promise<[address: string, balance: BN]> => {
  const address = getAddressFromPrivateKey(privateKey);
  let zilliqa = new Zilliqa(hre.getNetworkUrl());

  const balanceResult = await zilliqa.blockchain.getBalance(address);
  if (balanceResult.error) {
    return [address, new BN(0)];
  } else {
    return [address, new BN(balanceResult.result.balance)];
  }
};

export const getZilAddress = (privateKey: string): string => {
  return getAddressFromPrivateKey(privateKey).toLowerCase();
};

export const getEthAddress = (privateKey: string): string => {
  const wallet = new ethers.Wallet(privateKey);
  return wallet.address;
};
