import hre from "hardhat";
import { ethers } from "hardhat";
import {Wallet} from "ethers";
import clc from "cli-color";

async function createSigners() {
  const [signer] = await ethers.getSigners();
  const newSigners = Array.from({length: 20}, (v, k) => Wallet.createRandom().connect(ethers.provider));
  const BatchTransferContract = await ethers.getContractFactory("BatchTransfer");
  const batchTransfer = await BatchTransferContract.connect(signer).deploy({
    value: ethers.utils.parseUnits("20", "ether")
  });
  await batchTransfer.deployed();
  const addresses = newSigners.map((signer) => signer.address);
  await batchTransfer.batchTransfer(addresses, ethers.utils.parseUnits("1", "ether"));

  for (let s of newSigners) {
    console.log(`"${s.privateKey.substring(2)}",`)
  }
}

async function main() {
  await createSigners()
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
