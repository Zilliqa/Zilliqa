import clc from "cli-color";
import {task} from "hardhat/config";
import {Account, AccountType, getEthSignersBalances, getZilSignersBalances} from "../helpers/SignersHelper";
import {BN} from "@zilliqa-js/zilliqa";
import select, {Separator} from "@inquirer/select";
import input from "@inquirer/input";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import {ethers} from "ethers";
import {askAmount, askForAccount, askForAddress} from "../helpers/UiHelper";

enum WhatDoYouWantToDo {
  TransferFund,
  PrintBalances
}

task("ui", "A task to setup test suite").setAction(async (taskArgs, hre) => {
  const task: WhatDoYouWantToDo = await askWhatDoYouWantToDo();

  switch (task) {
    case WhatDoYouWantToDo.TransferFund:
      await transferFund(hre);
      break;

    case WhatDoYouWantToDo.PrintBalances:
      await printBalances(hre);
      break;

    default:
      console.log(clc.yellow("Selected option is not valid!"));
      break;
  }
});

const askWhatDoYouWantToDo = async (): Promise<WhatDoYouWantToDo> => {
  return await select({
    message: "What do you want to do?",
    choices: [
      {
        name: "Transfer fund",
        value: WhatDoYouWantToDo.TransferFund
      },
      {
        name: "Print signers balances",
        value: WhatDoYouWantToDo.PrintBalances
      }
    ]
  });
};

const askForAccountType = async (message?: string, ethMessage?: string, zilMessage?: string): Promise<AccountType> => {
  return await select({
    message: message || "What type of funded source account do you have?",
    choices: [
      {
        name: ethMessage || "An eth-based account",
        value: AccountType.EthBased
      },
      {
        name: zilMessage || "A zil-based account",
        value: AccountType.ZilBased
      }
    ]
  });
};

async function transferFund(hre: HardhatRuntimeEnvironment) {
  const from = await askForAccount();
  const to = await askForAddress();
  const amount = await askAmount("How much to transfer? ");

  await hre.run("transfer", {
    from: from.private_key,
    to: to,
    amount: ethers.utils.formatEther(amount),
    fromAddressType: from.type as string
  });
}

async function printBalances(hre: HardhatRuntimeEnvironment) {
  const accountType = await askForAccountType("Eth balances or Zil balances?", "ETH", "ZIL");

  await hre.run("balances", {zil: accountType === AccountType.ZilBased, eth: accountType === AccountType.EthBased});
}
