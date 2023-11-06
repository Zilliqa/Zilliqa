import clc from "cli-color";
import {task} from "hardhat/config";
import {AccountType, getEthSignersBalances, getZilSignersBalances} from "../helpers/SignersHelper";
import {BN} from "@zilliqa-js/zilliqa";
import select, {Separator} from "@inquirer/select";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import {ethers} from "ethers";
import {askAmount, askForAccount, askForAccountType, askForAddress} from "../helpers/UiHelper";

enum WhatDoYouWantToDo {
  RunTestsSequentially,
  RunTestsInParallel,
  RefundCurrentSigners,
}

task("setup", "A task to setup test suite").setAction(async (taskArgs, hre) => {
  const task: WhatDoYouWantToDo = await askWhatDoYouWantToDo();

  switch (task) {
    case WhatDoYouWantToDo.RefundCurrentSigners:
      await refundCurrentSigners(hre);
      break;

    case WhatDoYouWantToDo.RunTestsInParallel:
      await prepareToRunTestsInParallel(hre);
      break;

    case WhatDoYouWantToDo.RunTestsSequentially:
      await prepareToRunTestsSequentially(hre);
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
        name: "Run tests sequentially",
        value: WhatDoYouWantToDo.RunTestsSequentially
      },
      {
        name: "Run tests in parallel",
        value: WhatDoYouWantToDo.RunTestsInParallel
      },
      new Separator(),
      {
        name: "Refund current signers",
        value: WhatDoYouWantToDo.RefundCurrentSigners
      },
    ]
  });
};

const refundCurrentSigners = async (hre: HardhatRuntimeEnvironment) => {
  let amount = await askAmount();
  let account = await askForAccount();
  await hre.run("refund-signers", {
    from: account.private_key,
    fromAddressType: account.type as string,
    amount: amount.toString()
  });
};

async function prepareToRunTestsInParallel(hre: HardhatRuntimeEnvironment) {
  const NEEDED_SIGNERS = 30;
  const NEEDED_BALANCE = 1000;

  await prepareToRunTests(hre, NEEDED_SIGNERS, NEEDED_BALANCE);
  console.log(clc.greenBright("\nYou're good to go!"));
  console.log(clc.yellowBright.bold("\nrun: npx hardhat test --parallel"));
}

async function prepareToRunTestsSequentially(hre: HardhatRuntimeEnvironment) {
  const NEEDED_SIGNERS = 4;
  const NEEDED_BALANCE = 1000;

  await prepareToRunTests(hre, NEEDED_SIGNERS, NEEDED_BALANCE);
  console.log(clc.greenBright("\nYou're good to go."));
  console.log(clc.yellowBright.bold("\nrun: npx hardhat test"));
}

async function prepareToRunTests(hre: HardhatRuntimeEnvironment, needed_signers: number, needed_balance: number) {
  const balanceInWei = hre.ethers.utils.parseEther(needed_balance.toString());
  const ethBalances = await getEthSignersBalances(hre);
  const zilBalances = await getZilSignersBalances(hre);
  const signersAreEnough = ethBalances.length >= needed_signers;
  const ethSignersHaveEnoughFund = ethBalances.every(([_, balance]) => balance.gte(balanceInWei));
  const zilSignersHaveEnoughFund = zilBalances.every(([_, balance]) => balance.gte(new BN(balanceInWei.toString())));

  if (signersAreEnough && ethSignersHaveEnoughFund && zilSignersHaveEnoughFund) {
    return;
  }

  const sourceAccount = await askForAccount();

  await hre.run("refund-signers", {
    from: sourceAccount.private_key,
    fromAddressType: sourceAccount.type as string,
    amount: needed_balance.toString()
  });

  if (signersAreEnough == false) {
    await hre.run("init-signers", {
      from: sourceAccount.private_key,
      count: (needed_signers - ethBalances.length).toString(),
      balance: needed_balance.toString(),
      append: false,
      fromAddressType: sourceAccount.type as string
    });
  }
}