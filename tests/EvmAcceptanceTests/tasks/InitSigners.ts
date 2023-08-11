import {ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import fs from "fs";

task("initSigners", "A task to init signers")
  .addParam("from", "Sender's private key")
  .addParam("count", "Number of signers to be generated")
  .addParam("balance", "Balance of each newly generated signers")
  .setAction(async (taskArgs, hre) => {
    const {from, count, balance} = taskArgs;
    const accounts: string[] = [];
    for (let i = 0; i < Number(count); ++i) {
      const account = ethers.Wallet.createRandom();
      await fundEth(hre, from, account.address, balance);
      accounts.push(account.privateKey);
    }

    fs.writeFileSync(".signers", accounts.join("\n"));
  });

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
