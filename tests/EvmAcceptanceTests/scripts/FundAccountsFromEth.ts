import {Account} from "@zilliqa-js/zilliqa";
import hre, {ethers} from "hardhat";

async function main() {
  const signers = await ethers.getSigners();
  const private_keys: string[] = hre.network["config"]["accounts"] as string[];

  console.log("");
  console.log("Starting transfer...");

  let txns = [];
  for (const pv_index in private_keys) {
    const privateKey = private_keys[pv_index];
    const ethSigner = signers[pv_index];

    // Get corresponding eth account
    const zilAccount = new Account(privateKey);
    console.log("Account to send to: ", zilAccount.address);

    const signerBalance = await ethSigner.getBalance();
    if (signerBalance.isZero()) {
      console.log(`Skipping eth signer: ${ethSigner.address} with zero balance`);
      continue;
    }

    txns.push(
      ethSigner.sendTransaction({
        to: zilAccount.address.toLowerCase(),
        value: signerBalance.div(2)
      })
    );
  }

  await Promise.all(txns);

  console.log("");
  await hre.run("balances", {zil: true});
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
