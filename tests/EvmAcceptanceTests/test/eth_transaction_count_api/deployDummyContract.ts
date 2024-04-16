// deployContract.ts
import { ethers } from "hardhat";
import { assert, expect } from "chai";
import fs from 'fs';
import path from 'path';

describe("Dummy contract deployment for GetEthTransactionCount api test", function () {
    it("should deploy the contract", async function () {
        const Factory = await ethers.getContractFactory("Dummy");
        const contract = await Factory.deploy();
        await contract.deployed();

        console.log(`Contract deployed to: ${contract.address}`);
        expect(contract.address).to.be.properAddress;
        console.log("writing to file", __dirname);
        fs.writeFileSync(path.join(__dirname, './contractAddress.txt'), contract.address);

    });
});
