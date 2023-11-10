import clc from "cli-color";
import {task} from "hardhat/config";
import {ethers} from "ethers";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import {units} from "@zilliqa-js/zilliqa";
import {getEthSignersBalances, getZilBalance, getZilSignersBalances} from "../helpers/SignersHelper";

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
  const balances = await getEthSignersBalances(hre);

  console.log(clc.bold.bgGreen("Eth balances"));
  balances.forEach(([address, balance], index) => {
    displayBalance(index, address, ethers.utils.formatEther(balance), "ether");
  });
  console.log();
};

const printZilBalances = async (hre: HardhatRuntimeEnvironment) => {
  const balances = await getZilSignersBalances(hre);

  let index = 1;

  console.log(clc.bold.bgGreen("Zil balances"));
  for (const [address, balance] of balances) {
    const balanceString = balance.isZero() ? clc.red("Account is not created") : units.fromQa(balance, units.Units.Zil);
    const unit = balance.isZero() ? "" : "ZIL";
    displayBalance(++index, address, balanceString, unit);
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
