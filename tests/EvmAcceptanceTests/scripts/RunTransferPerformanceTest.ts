import axios from "axios";
import hre, {ethers, Web3} from "hardhat";
import clc from "cli-color";
import {Wallet} from "ethers";
const DEST_ACCOUNTS_NUM = 1000;

async function main() {
  const privateKey = process.env.PRIMARY_ACCOUNT;
  if (privateKey === undefined) {
    console.log(clc.bold.red("Please set PRIMARY_ACCOUNT environment variable before running this script."));
    return;
  }

  const signer = new ethers.Wallet(privateKey, ethers.provider);

  const balance = await signer.getBalance();

  if (balance.isZero()) {
    console.log(clc.bold.red("Signer doesn't have funds"));
    return;
  }

  console.log(clc.bold.white("Generating: " + DEST_ACCOUNTS_NUM + " random recipients..."));

  let recipients: Wallet[] = [];
  for (let i = 0; i < DEST_ACCOUNTS_NUM; i++) {
    recipients.push(ethers.Wallet.createRandom());
  }

  console.log(clc.bold.white("Generating: " + DEST_ACCOUNTS_NUM + " random recipients...done"));

  const web3 = new Web3(hre.getNetworkUrl());

  console.log(clc.bold.white("Starting main loop"));

  let id = 1;
  while (true) {
    let nonce = await signer.getTransactionCount();
    try {
      let toSign = [];

      for (let i = 0; i < recipients.length; ++i) {
        toSign.push(
          web3.eth.accounts.signTransaction(
            {
              to: recipients[i].address,
              value: "1000000",
              nonce: nonce + Math.floor(Math.random() * 2 * DEST_ACCOUNTS_NUM),
              gasPrice: "0x9184e72a000",
              gas: "0x5208",
              chainId: hre.getEthChainId(),
            },
            privateKey
          )
        );
      }

      console.log(clc.bold.white("Doing bulk sign..."));
      let responses = await Promise.all(toSign);
      console.log(clc.bold.white("Doing bulk sign...done"));

      let txns = [];

      for (let i = 0; i < responses.length; ++i) {
        const payload: Object = {
          jsonrpc: "2.0",
          method: "eth_sendRawTransaction",
          params: [responses[i].rawTransaction],
          id: id++
        };
        txns.push(axios.post(hre.getNetworkUrl(), payload));
      }

      await Promise.all(txns);
    } catch (e) {}
    console.log(clc.bold.white("Doing bulk send...done"));
    await new Promise((r) => setTimeout(r, 5000));
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
