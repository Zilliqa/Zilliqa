import {BigNumber, ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import clc from "cli-color";
import ora from "ora";
import {getAddressFromPrivateKey} from "@zilliqa-js/zilliqa";
import {loadFromSignersFile, loadSignersFromConfig} from "../helpers/SignersHelper";

task("refund-signers", "A task to refund signers")
  .addParam("from", "Sender's private key")
  .addParam("amount", "Balance of each newly generated signers")
  .setAction(async (taskArgs, hre) => {
    const {from, amount} = taskArgs;

    const spinner = ora();
    const accounts = [...loadFromSignersFile(hre.network.name), ...loadSignersFromConfig(hre)]
    if (accounts.length === 0) {
      console.log(clc.yellowBright(`No signers found in .signers/${hre.network.name}`));
      return;
    }

    spinner.start(`Refunding ${accounts.length} accounts for ${hre.network.name} network...`);
    await refundAccounts(hre, from, hre.ethers.utils.parseEther(amount), accounts);

    spinner.succeed();
  });

const refundAccounts = async (
  hre: HardhatRuntimeEnvironment,
  privateKey: string,
  amount: BigNumber,
  accounts: string[]
) => {
  const wallet = new ethers.Wallet(privateKey, hre.ethers.provider);

  const addresses = [
    ...accounts.map((private_key) => new ethers.Wallet(private_key).address.toLocaleLowerCase()),
    ...accounts.map((private_key) => getAddressFromPrivateKey(private_key).toLocaleLowerCase())
  ];

  const totalValue = amount.mul(addresses.length);
  if ((await wallet.getBalance()).lt(totalValue)) {
    throw new Error("Sender doesn't have enough fund in its eth address.");
  }


  let contract = await hre.deployContractWithSigner("BatchTransferCtor", wallet, addresses, amount, {
    value: totalValue
  });

  await contract.deployed();
};
