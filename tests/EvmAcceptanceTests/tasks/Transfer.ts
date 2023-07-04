import { BN, Zilliqa, bytes, getAddressFromPrivateKey, toChecksumAddress } from "@zilliqa-js/zilliqa";
import clc from "cli-color";
import { ethers } from "ethers";
import { task } from "hardhat/config";
import { HardhatRuntimeEnvironment } from "hardhat/types";
import Long from "long";

task("transfer", "A task to transfer fund")
  .addParam("from", "Sender's private key")
  .addParam("to", "Receiver's address")
  .addParam("amount", "Amount to be transfered")
  .addParam("addressType", "It can be either `eth` or `zil`. If eth is selected, Eth address of private key will be used. Otherwise, the zil address will be used.")
  .setAction(async (taskArgs, hre) => {
    const { from, to, amount, addressType } = taskArgs;
    if (addressType === "eth") {
      await fundEth(hre, from, to, amount);
    } else if (addressType === "zil") {
      await fundZil(hre, from, to, amount);
  } else {
    throw new Error(`--address-type should be either eth or zil. ${addressType} is not supported`)
  }
});

const fundEth = async (hre: HardhatRuntimeEnvironment, privateKey: string, to: string, amount: string) => {
  const provider = new ethers.providers.JsonRpcProvider(hre.getNetworkUrl());
  const wallet = new ethers.Wallet(privateKey, provider); 
  console.log(`Current balance: ${clc.yellow.bold(await provider.getBalance(to))}`)
  await wallet.sendTransaction({
    to: to.toString(),
    value: ethers.BigNumber.from(amount)
  })
  console.log(`New balance:     ${clc.green.bold(await provider.getBalance(to))}`)
}

const fundZil = async (hre: HardhatRuntimeEnvironment, privateKey: string, to: string, amount: string) => {
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  const msgVersion = 1; // current msgVersion
  const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);
  zilliqa.wallet.addByPrivateKey(privateKey);
  const ethAddrConverted = toChecksumAddress(to); // Zil checksum
  const balance = (await zilliqa.blockchain.getBalance(to)).result.balance;
  console.log(`Current balance: ${clc.yellow.bold(balance)}`)
  const tx = await zilliqa.blockchain.createTransactionWithoutConfirm(
    zilliqa.transactions.new(
      {
        version: VERSION,
        toAddr: ethAddrConverted,
        amount: new BN(amount), // Sending an amount in Zil (1) and converting the amount to Qa
        gasPrice: new BN(2000000000), // Minimum gasPrice veries. Check the `GetMinimumGasPrice` on the blockchain
        gasLimit: Long.fromNumber(2100)
      },
      false
    )
  );

  if (tx.id) {
    const confirmedTxn = await tx.confirm(tx.id);
    const receipt = confirmedTxn.getReceipt();
    if (receipt && receipt.success) {
      const balance = (await zilliqa.blockchain.getBalance(to)).result.balance;
      console.log(`New balance:     ${clc.green.bold(balance)}`)
      return;
    }
  }

  console.log(clc.red(`Failed to fund ${ethAddrConverted}.`))
}