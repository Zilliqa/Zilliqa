import { ethers } from "hardhat";
import { expect } from "chai";
import fs from 'fs';
import path from 'path';


describe("Execute txn test for GetEthTransactionCount api test", function () {
    it("should execute transaction ", async function () {
        const address = fs.readFileSync(path.join(__dirname, '/contractAddress.txt'), 'utf8').trim();


        const Contract = await ethers.getContractFactory("Dummy");
        const contract = Contract.attach(address);

        const [owner] = await ethers.getSigners();
        const txn = await contract.connect(owner).dummy();
        const receipt = await txn.wait();

        console.log("Transaction executed:", txn.hash);
        console.log("Mined in block:", receipt.blockNumber);

        expect(receipt).to.not.be.undefined;
    });
});
