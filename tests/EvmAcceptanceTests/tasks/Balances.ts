import clc from "cli-color";
import {task} from "hardhat/config";
import {ethers} from "ethers";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import {BN, Zilliqa, getAddressFromPrivateKey, units} from "@zilliqa-js/zilliqa";

task("balances", "A task to get balances of signers in the config")
  .addFlag("zil", "Show balances in zil based addresses of private keys")
  .addFlag("eth", "Show balances in eth based addresses of private keys")
  .setAction(async (taskArgs, hre) => {
    const {zil, eth} = taskArgs;
    if (eth) {
      await printEthBalances(hre);
    }

    if (zil) {
      await printZilBalances(hre);
    }

    if (!zil && !eth) {
      await printEthBalances(hre);
    }
  });

const printEthBalances = async (hre: HardhatRuntimeEnvironment) => {
  const {provider} = hre.network;

  const accounts: string[] = await provider.send("eth_accounts");
  const balances = await Promise.all(
    accounts.map((account: string) => provider.send("eth_getBalance", [account, "latest"]))
  );

  console.log(clc.bold.bgGreen("Eth balances"));
  accounts.forEach((element, index) => {
    displayBalance(index, element, ethers.utils.formatEther(balances[index]), "ether");
  });
  console.log();
};

const printZilBalances = async (hre: HardhatRuntimeEnvironment) => {
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  const private_keys: string[] = hre.network["config"]["accounts"] as string[];
  let index = 0;
  for (const private_key of private_keys) {
    const address = getAddressFromPrivateKey(private_key);

    const balanceResult = await zilliqa.blockchain.getBalance(address);
    let balanceString: string;
    let error = false;
    if (balanceResult.error) {
      error = true;
      balanceString = clc.red.bold(balanceResult.error.message);
    } else {
      const balance = new BN(balanceResult.result.balance);
      balanceString = units.fromQa(balance, units.Units.Zil);
    }

    displayBalance(++index, address, balanceString, error ? "" : "zil");
  }
};

const displayBalance = (index: number, account: string, balance: string, unit: string) => {
  console.log(
    clc.blackBright(`${index + 1})`),
    clc.white.bold(account),
    clc.blackBright("\tBalance:"),
    clc.white.bold(balance),
    clc.blackBright(unit)
  );
};
