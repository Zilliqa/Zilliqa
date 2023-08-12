import {ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import fs from "fs";
import {join} from "path";
import clc from "cli-color";
import ora from "ora";

task("init-signers", "A task to init signers")
  .addParam("from", "Sender's private key")
  .addParam("count", "Number of signers to be generated")
  .addParam("balance", "Balance of each newly generated signers")
  .addFlag("append", "Append new signers to the end of the .signer-<network> file")
  .setAction(async (taskArgs, hre) => {
    const {from, count, balance, append} = taskArgs;
    const accounts: string[] = [];

    for (let i = 0; i < Number(count); ++i) {
      const spinner = ora();
      const account = ethers.Wallet.createRandom();
      spinner.start(
        `${i + 1})\t${clc.bold(`${account.address}`)} ${clc.blackBright(`created. Funding ${balance} eth...`)}`
      );
      await fundEth(hre, from, account.address, balance);
      spinner.succeed();
      accounts.push(account.privateKey);
    }

    const file_name = `${hre.network.name}.json`;

    try {
      await writeToFile(accounts, append, file_name);
      console.log();
      console.log(
        clc.bold(`.signers/${file_name}`),
        clc.blackBright(`${append ? "updated" : "created"} successfully.`)
      );
    } catch (error) {
      console.log(clc.red(error));
    }
  });

const writeToFile = async (signers: string[], append: boolean, file_name: string) => {
  await fs.promises.mkdir(".signers", {recursive: true});
  const current_signers: string[] = append
    ? JSON.parse(await fs.promises.readFile(join(".signers", file_name), "utf8"))
    : [];

  current_signers.push(...signers);
  await fs.promises.writeFile(join(".signers", file_name), JSON.stringify(current_signers));
};

const fundEth = async (hre: HardhatRuntimeEnvironment, privateKey: string, to: string, amount: string) => {
  const provider = new ethers.providers.JsonRpcProvider(hre.getNetworkUrl());
  const wallet = new ethers.Wallet(privateKey, provider);
  if ((await wallet.getBalance()).isZero()) {
    throw new Error("Sender doesn't have enough fund in its eth address.");
  }

  const response = await wallet.sendTransaction({
    to: to.toString(),
    value: ethers.utils.parseEther(amount)
  });

  return response.wait(); // Wait for transaction receipt
};
