import clc from "cli-color";
import {task} from "hardhat/config";
import {terminal} from "terminal-kit";

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

task("setup", "A task to setup test suite").setAction(async (taskArgs, hre) => {
  const address_type = await askAddressType();
  if (address_type === undefined) {
    return;
  }

  const private_key = await askPrivateKey();

  if (private_key === undefined) {
    return;
  }

  const append = await askAppendNewSignersToFile();

  if (append === undefined) {
    return;
  }

  if (address_type == AddressType.EvmBased) {
    await hre.run("init-signers", {from: private_key, count: "30", balance: "10", append});
  } else if (address_type == AddressType.ZilBased) {
    console.log("To be supported");
  }
});
