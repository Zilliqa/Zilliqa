import {BN, Zilliqa, getAddressFromPrivateKey} from "@zilliqa-js/zilliqa";
import clc from "cli-color";
import {task} from "hardhat/config";

task("zilBalance", "A task to get balance of a private key")
  .addPositionalParam("privateKey")
  .setAction(async (taskArgs, hre) => {
    let zilliqa = new Zilliqa(hre.getNetworkUrl());
    const privateKey = taskArgs.privateKey;
    const address = getAddressFromPrivateKey(privateKey);

    console.log(`Address: ${address}`);
    const balanceResult = await zilliqa.blockchain.getBalance(address);
    if (balanceResult.error) {
      console.log(clc.bold.red(balanceResult.error.message));
      return;
    }

    const balance = new BN(balanceResult.result.balance);
    console.log(`Balance: ${clc.bold.green(balance)}`);
  });
