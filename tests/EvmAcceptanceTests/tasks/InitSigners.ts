import {BigNumber, ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import fs from "fs";
import {join} from "path";
import clc from "cli-color";
import ora from "ora";
import {getAddressFromPrivateKey} from "@zilliqa-js/zilliqa";

task("init-signers", "A task to init signers")
  .addParam("from", "Sender's private key")
  .addParam("count", "Number of signers to be generated")
  .addParam("balance", "Balance of each newly generated signers")
  .addFlag("append", "Append new signers to the end of the .signer-<network> file")
  .setAction(async (taskArgs, hre) => {
    const {from, count, balance, append} = taskArgs;

    const spinner = ora();
    spinner.start(`Creating ${count} accounts...`);

    const accounts = await createAccountsEth(hre, from, hre.ethers.utils.parseEther(balance), count);

    spinner.succeed();

    const file_name = `${hre.network.name}.json`;

    try {
      await writeToFile(
        accounts.map((account) => account.privateKey),
        append,
        file_name
      );
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

const createAccountsEth = async (
  hre: HardhatRuntimeEnvironment,
  privateKey: string,
  amount: BigNumber,
  count: number
) => {
  const wallet = new ethers.Wallet(privateKey, hre.ethers.provider);

  if ((await wallet.getBalance()).isZero()) {
    throw new Error("Sender doesn't have enough fund in its eth address.");
  }

  const accounts = Array.from({length: count}, (v, k) => ethers.Wallet.createRandom().connect(hre.ethers.provider));

  const addresses = [
    ...accounts.map((signer) => signer.address),
    ...accounts.map((signer) => getAddressFromPrivateKey(signer.privateKey).toLocaleLowerCase())
  ];

  await hre.deployContractWithSigner("BatchTransferCtor", wallet, addresses, amount, {
    value: amount.mul(addresses.length)
  });

  return accounts;
};
