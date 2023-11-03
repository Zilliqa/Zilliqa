import {BigNumber, ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import clc from "cli-color";
import ora from "ora";
import {BN, getAddressFromPrivateKey} from "@zilliqa-js/zilliqa";
import {
  Account,
  AccountType,
  getEthAddress,
  getEthSignersBalances,
  getZilSignersBalances,
  loadFromSignersFile,
  loadSignersFromConfig
} from "../helpers/SignersHelper";

task("refund-signers", "A task to refund signers")
  .addParam("from", "Sender's private key")
  .addParam(
    "fromAddressType",
    "It can be either `eth` or `zil`. If eth is selected, Eth address of private key will be used. Otherwise, the zil address will be used."
  )
  .addParam("amount", "Balance of each newly generated signers")
  .setAction(async (taskArgs, hre) => {
    const {from, fromAddressType, amount} = taskArgs;

    const spinner = ora();

    spinner.start(`Refunding signers for ${hre.network.name} network...`);

    if (fromAddressType === "eth" || fromAddressType === "zil") {
      await refundAccounts(
        hre,
        {private_key: from, type: fromAddressType as AccountType},
        hre.ethers.utils.parseEther(amount)
      );
    } else {
      displayError(`--from-address-type should be either eth or zil. ${fromAddressType} is not supported`);
      spinner.fail();
      return;
    }

    spinner.succeed();
  });

const refundAccounts = async (hre: HardhatRuntimeEnvironment, sourceAccount: Account, amount: BigNumber) => {
  const wallet = new ethers.Wallet(sourceAccount.private_key, hre.ethers.provider);

  const ethBalances = await getEthSignersBalances(hre);
  const zilBalances = await getZilSignersBalances(hre);

  const addresses = [
    ...ethBalances.filter(([address, balance]) => balance.lt(amount)).map(([address, _]) => address),
    ...zilBalances.filter(([address, balance]) => balance.lt(new BN(amount.toString()))).map(([address, _]) => address)
  ];

  const totalValue = amount.mul(addresses.length);

  if (sourceAccount.type === AccountType.ZilBased) {
    await hre.run("transfer", {
      from: sourceAccount.private_key,
      to: getEthAddress(sourceAccount.private_key),
      amount: ethers.utils.formatEther(amount.add(totalValue)), // Add +amount for the source account itself
      fromAddressType: "zil"
    });
  }

  if ((await wallet.getBalance()).lt(totalValue)) {
    throw new Error("Sender doesn't have enough fund in its eth address.");
  }

  let contract = await hre.deployContractWithSigner("BatchTransferCtor", wallet, addresses, amount, {
    value: totalValue
  });

  await contract.deployed();
};

// TODO: DRY
const displayError = (error: string) => {
  console.log(clc.red.bold("Error: "), error);
};
