import hre, {web3} from "hardhat";
import clc from "cli-color";
import {bytes, toChecksumAddress, Zilliqa} from "@zilliqa-js/zilliqa";
import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
const {BN, Long} = require("@zilliqa-js/util");

async function main() {
  const {provider} = hre.network;

  // Constants needed for zil transfer
  const msgVersion = 1; // current msgVersion
  const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);

  const accounts: string[] = await provider.send("eth_accounts");
  let balances = await Promise.all(
    accounts.map((account: string) => provider.send("eth_getBalance", [account, "latest"]))
  );

  // Print balances of eth accounts before
  accounts.forEach((element, index) => {
    console.log(
      clc.bold("Account"),
      clc.green(element),
      clc.bold("Initial balance:"),
      clc.greenBright(balances[index])
    );
  });

  const private_keys: string[] = hre.network["config"]["accounts"] as string[];
  for (const element of private_keys) {
    console.log("");
    console.log("Starting transfer...");

    // Get corresponding eth account
    let ethAddr = web3.eth.accounts.privateKeyToAccount(element);
    let ethAddrConverted = toChecksumAddress(ethAddr.address); // Zil checksum
    let initialAccountBal = await web3.eth.getBalance(ethAddr.address);
    console.log("Account to send to (zil checksum): ", ethAddrConverted);
    console.log("Account to send to, initial balance : ", initialAccountBal);

    // Transfer half funds to this account
    let zilliqa = new Zilliqa(hre.getNetworkUrl());
    zilliqa.wallet.addByPrivateKey(element);
    const address = getAddressFromPrivateKey(element);
    console.log(`My ZIL account address is: ${address}`);

    const res = await zilliqa.blockchain.getBalance(address);

    if (res.error?.message) {
      console.log("Error: ", res.error);
      console.log("Skipping account with error");
      continue;
    }
    const balance = res.result.balance;

    console.log(`My ZIL account balance is: ${balance}`);

    if (balance == 0) {
      console.log("Skipping account with 0 balance");
      continue;
    }

    const gasp = await web3.eth.getGasPrice();
    const gasPrice = new BN(gasp);

    const tx = await zilliqa.blockchain.createTransactionWithoutConfirm(
      zilliqa.transactions.new(
        {
          version: VERSION,
          toAddr: ethAddrConverted,
          amount: new BN(balance).div(new BN(2)), // Sending an amount in Zil (1) and converting the amount to Qa
          gasPrice: gasPrice, // Minimum gasPrice veries. Check the `GetMinimumGasPrice` on the blockchain
          gasLimit: Long.fromNumber(2100)
        },
        false
      )
    );

    // process confirm
    if (tx.id) {
      console.log(`The transaction id is:`, tx.id);
      const confirmedTxn = await tx.confirm(tx.id);

      console.log(`The transaction status is:`);
      console.log(confirmedTxn.getReceipt());

      let finalBal = await web3.eth.getBalance(ethAddr.address);
      console.log(`My new account balance is: ${finalBal}`);
    } else {
      console.log("Failed");
    }
  }

  balances = await Promise.all(accounts.map((account: string) => provider.send("eth_getBalance", [account, "latest"])));

  // Print balances of eth accounts before
  accounts.forEach((element, index) => {
    console.log(
      clc.bold("Account"),
      clc.green(element),
      clc.bold("Initial balance:"),
      clc.greenBright(balances[index])
    );
  });
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
