import clc from "cli-color";
import {task} from "hardhat/config";
import {terminal} from "terminal-kit";
import {getEthAddress, getEthBalance, getZilAddress, getZilBalance} from "./helpers";
import {BN} from "@zilliqa-js/zilliqa";

enum AddressType {
  EvmBased,
  ZilBased
}

const displayTitle = (title: string) => {
  terminal.white.bold(`${title} ${clc.blackBright("(Press ESC to exit)")}\n`);
};

const askAddressType = async (): Promise<AddressType | undefined> => {
  console.clear();
  var items = [
    "An account(private key) with some ethers in its ethereum based address",
    "An account(private key) with some zils in its zilliqa based address"
  ];

  displayTitle("What do you have?");
  let out = await terminal.singleColumnMenu(items, {cancelable: true}).promise;
  return out.canceled ? undefined : out.selectedIndex;
};

const askAppendNewSignersToFile = async (): Promise<boolean | undefined> => {
  console.clear();
  var items = ["Create a new signers file(Replaces the previous ones)", "Append new signers to the previous ones"];

  displayTitle("Create a new signers file?");
  let out = await terminal.singleColumnMenu(items, {cancelable: true}).promise;
  return out.canceled ? undefined : out.selectedIndex === 1;
};

const askPrivateKey = async (): Promise<string | undefined> => {
  console.clear();
  displayTitle("Please enter your private key: ");
  let out = terminal.inputField({cancelable: true});
  const address = await out.promise;

  return address;
};

const askSignersCount = async (): Promise<number | undefined> => {
  console.clear();
  displayTitle("Please enter number signers to create: ");
  let out = terminal.inputField({cancelable: true, default: "30"});
  const count = await out.promise;
  if (count === undefined) {
    return undefined;
  }

  return Number(count);
};

const askBalance = async (): Promise<number | undefined> => {
  console.clear();
  displayTitle("Please enter amount of balance for each signer in Ether: ");
  let out = terminal.inputField({cancelable: true, default: "1000"});
  const balance = await out.promise;
  if (balance === undefined) {
    return undefined;
  }

  return Number(balance);
};

task("setup", "A task to setup test suite").setAction(async (taskArgs, hre) => {
  const address_type = await askAddressType();
  if (address_type === undefined) {
    return;
  }

  const private_key = await askPrivateKey();

  if (private_key === undefined) {
    return;
  }

  const signersCount = await askSignersCount();

  if (signersCount === undefined) {
    return;
  }

  const eachSignerBalance = await askBalance();

  if (eachSignerBalance === undefined) {
    return;
  }

  const append = await askAppendNewSignersToFile();

  if (append === undefined) {
    return;
  }

  if (address_type == AddressType.EvmBased) {
    // *2 because we both fund zil/eth address.
    const neededBalance = eachSignerBalance * 2 * signersCount;
    const [address, balance] = await getEthBalance(hre, private_key);
    if (balance.isZero()) {
      console.log(
        clc.red(`Provided private key with address ${address} does not enough funds. Needed ${neededBalance} ZIL.`)
      );
      return;
    }
    await hre.run("init-signers", {from: private_key, count: "30", balance: eachSignerBalance.toString(), append});
  } else if (address_type == AddressType.ZilBased) {
    // *2 because we both fund zil/eth address. +1 to fund the private key eth-address as well.
    const neededBalance = eachSignerBalance * 2 * (signersCount + 1);

    const [address, balance] = await getZilBalance(hre, private_key);
    if (balance.lt(new BN(neededBalance))) {
      console.log(
        clc.red(`Provided private key with address ${address} does not enough funds. Needed ${neededBalance} ZIL.`)
      );
      return;
    }

    // First fund the eth address of the private key
    await hre.run("transfer", {
      from: private_key,
      to: getEthAddress(private_key),
      amount: neededBalance.toString(),
      fromAddressType: "zil"
    });

    // Then fund them in parallel
    await hre.run("init-signers", {
      from: private_key,
      count: signersCount.toString(),
      balance: eachSignerBalance.toString(),
      append
    });
  }
});
