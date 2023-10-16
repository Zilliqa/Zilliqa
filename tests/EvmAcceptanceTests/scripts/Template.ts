import {TransactionRequest} from "@ethersproject/providers";
import {Contract, Transaction} from "ethers";
import {ethers} from "hardhat";

async function deployContract(contractName: string): Promise<Contract> {
  const Contract = await ethers.getContractFactory(contractName);
  const contract = await Contract.deploy();
  await contract.deployed();
  return contract;
}

async function sendTransaction(tx: TransactionRequest): Promise<Transaction> {
  const [signer] = await ethers.getSigners();
  return signer.sendTransaction(tx);
}

async function main() {
  /*
   * 1. Copy this file to something like MyScript.ts.
   * 2. Put your contract inside `contracts` folder.
   * 3. Put your code inside this main function. Use `deployContract` or `sendTransaction` helper functions.
   * 4. Run it like: `npx hardhat run scripts/MyScript.ts --network yournetwork`
   */
  console.log("IT WORKS!");
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
