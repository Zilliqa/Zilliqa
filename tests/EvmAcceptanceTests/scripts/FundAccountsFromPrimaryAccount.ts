import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import {BN, Zilliqa, bytes, toChecksumAddress} from "@zilliqa-js/zilliqa";
import clc from "cli-color";
import {ethers} from "ethers";
import Long from "long";
import hre from "hardhat";

// Refer to README.md, section `Testing a newly deployed testnet/devent` for more info.

async function main() {
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  const msgVersion = 1; // current msgVersion
  const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);

  const privateKey = process.env.PRIMARY_ACCOUNT;
  if (privateKey === undefined) {
    console.log(clc.bold.red("Please set PRIMARY_ACCOUNT environment variable before running this script."));
    return;
  }

  const address = getAddressFromPrivateKey(privateKey);

  console.log(`Private key: ${privateKey}`);
  console.log(`Address: ${address}`);
  const balanceResult = await zilliqa.blockchain.getBalance(address);

  if (balanceResult.error) {
    console.log(clc.bold.red(balanceResult.error.message));
    return;
  }

  const balance = new BN(balanceResult.result.balance);
  console.log(`Balance: ${clc.bold.green(balance)}`);

  if (balance.isZero()) {
    console.log(clc.bold.red("Provided account doesn't have enough fund."));
    return;
  }

  zilliqa.wallet.addByPrivateKey(privateKey);
  const private_keys: string[] = hre.network["config"]["accounts"] as string[];
  for (const element of private_keys) {
    const wallet = new ethers.Wallet(element);
    let ethAddrConverted = toChecksumAddress(wallet.address); // Zil checksum
    const tx = await zilliqa.blockchain.createTransactionWithoutConfirm(
      zilliqa.transactions.new(
        {
          version: VERSION,
          toAddr: ethAddrConverted,
          amount: new BN("1_000_000_000_00"), // Sending an amount in Zil (1) and converting the amount to Qa
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
        console.log(`${ethAddrConverted}` + clc.bold.green(" funded."));
        continue;
      }
    }

    console.log(clc.red(`Failed to fund ${ethAddrConverted}.`));
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
