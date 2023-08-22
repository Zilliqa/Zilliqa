import {BN, Zilliqa, bytes, toChecksumAddress, units} from "@zilliqa-js/zilliqa";
import clc from "cli-color";
import {ethers} from "ethers";
import {task} from "hardhat/config";
import {HardhatRuntimeEnvironment} from "hardhat/types";
import Long from "long";

task("transfer", "A task to transfer fund")
  .addParam("from", "Sender's private key")
  .addParam(
    "fromAddressType",
    "It can be either `eth` or `zil`. If eth is selected, Eth address of private key will be used. Otherwise, the zil address will be used."
  )
  .addParam("to", "Receiver's address")
  .addParam("amount", "Amount to be transferred")
  .setAction(async (taskArgs, hre) => {
    const {from, to, amount, fromAddressType} = taskArgs;
    if (fromAddressType === "eth") {
      await fundEth(hre, from, to.toLowerCase(), amount);
    } else if (fromAddressType === "zil") {
      await fundZil(hre, from, to.toLowerCase(), amount);
    } else {
      displayError(`--from-address-type should be either eth or zil. ${fromAddressType} is not supported`);
    }
  });

const fundEth = async (hre: HardhatRuntimeEnvironment, privateKey: string, to: string, amount: string) => {
  const provider = new ethers.providers.JsonRpcProvider(hre.getNetworkUrl());
  const wallet = new ethers.Wallet(privateKey, provider);
  if ((await wallet.getBalance()).isZero()) {
    displayError("Sender doesn't have enough fund in its eth address.");
    return;
  }

  console.log(`Current balance: ${clc.yellow.bold(await provider.getBalance(to))}`);
  const response = await wallet.sendTransaction({
    to: to.toString(),
    value: ethers.utils.parseEther(amount)
  });

  await response.wait(); // Wait for transaction receipt
  console.log(`New balance:     ${clc.green.bold(await provider.getBalance(to))}`);
};

const fundZil = async (hre: HardhatRuntimeEnvironment, privateKey: string, to: string, amount: string) => {
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  const msgVersion = 1; // current msgVersion
  const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);
  zilliqa.wallet.addByPrivateKey(privateKey);
  const ethAddrConverted = toChecksumAddress(to); // Zil checksum
  const balance = await getZilBalance(hre, to);
  console.log(`Current balance: ${clc.yellow.bold(balance)}`);

  const tx = await zilliqa.blockchain.createTransactionWithoutConfirm(
    zilliqa.transactions.new(
      {
        version: VERSION,
        toAddr: ethAddrConverted,
        amount: units.toQa(amount, units.Units.Zil),
        gasPrice: new BN(2000000000),
        gasLimit: Long.fromNumber(2100)
      },
      false
    )
  );

  if (tx.id) {
    const confirmedTxn = await tx.confirm(tx.id);
    const receipt = confirmedTxn.getReceipt();
    if (receipt && receipt.success) {
      const balance = await getZilBalance(hre, to);
      console.log(`New balance:     ${clc.green.bold(balance)}`);
      return;
    }
  }

  displayError(`Failed to fund ${ethAddrConverted}.`);
};

const displayError = (error: string) => {
  console.log(clc.red.bold("Error: "), error);
};

const getZilBalance = async (hre: HardhatRuntimeEnvironment, address: string): Promise<BN> => {
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  const balanceResult = await zilliqa.blockchain.getBalance(address);

  if (balanceResult.error) {
    return new BN(0);
  }

  return new BN(balanceResult.result.balance);
};
