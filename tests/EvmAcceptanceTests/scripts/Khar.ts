import hre, {ethers, web3} from "hardhat";
import clc from "cli-color";
import {bytes, toBech32Address, toChecksumAddress, units, Zilliqa} from "@zilliqa-js/zilliqa";
import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
const {BN, Long} = require("@zilliqa-js/util");

async function main() {
  const {provider} = hre.network;

  // Constants needed for zil transfer
  const msgVersion = 1; // current msgVersion
  const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);

  const [signer] = await ethers.getSigners();
  let zilliqa = new Zilliqa(hre.getNetworkUrl());
  for (const element of hre.network["config"]["accounts"]) {
    zilliqa.wallet.addByPrivateKey(element);
    const address = getAddressFromPrivateKey(element).toLowerCase();
    const addressEther = ethers.utils.getAddress(getAddressFromPrivateKey(element).toLowerCase());
    console.log(`My ZIL account address is: ${address}`);
    
    await signer.sendTransaction({
      to: addressEther,
      value: ethers.utils.parseEther("10000")
    })

    console.log(clc.red("----------------------------------------------------"));
    const balance = await zilliqa.blockchain.getBalance(address);
    console.log(`balance: ${JSON.stringify(balance.result.balance)}`);
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
