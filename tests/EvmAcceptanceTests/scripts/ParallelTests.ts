import { expect } from "chai";
import clc from "cli-color";
import { BigNumber, Contract } from "ethers";
import { ethers } from "hardhat";

type ContractInfo = {
    name: string;
    args: any[];
    value?: BigNumber
};

type Assertion = (value: any) => any;

type TransactionInfo = {
    txn: Promise<any>;
    assertion?: Assertion;
    msg?: string;
}

const deployContracts = async function (...contracts: ContractInfo[]) {
    let contractsPromises: Promise<Contract>[] = [];
    const [signer] = await ethers.getSigners();
    let nonce = await signer.getTransactionCount();
    for (const contract of contracts) {
        const Contract = await ethers.getContractFactory(contract.name);
        contractsPromises.push(Contract.deploy(...contract.args, { value: contract.value, nonce: nonce }));
        nonce += 1;
    }

    return Promise.all(contractsPromises);
}

const fillBlock = async function (...txns: TransactionInfo[]) {
    const values = await Promise.all(txns.map((txnInfo) => txnInfo.txn));
    for (const index in txns) {
        let txn = txns[index];
        if (txn.assertion)
        {

            try {
                await txn.assertion(values[index]);
                console.log(`${clc.green('✔')} ${clc.blackBright(txn.msg)}`);
            } catch (error) {
                console.log(`❌ ${clc.bold(txn.msg)}`);
            }
        }
    }
}

const test = function(msg: string, txn: Promise<any>, assertion: Assertion): TransactionInfo {
    return {
        msg,
        txn,
        assertion
    }
}

async function main() {
    const FUND = ethers.utils.parseUnits("1", "gwei");

    const contractsToDeploy: ContractInfo[] = [
        {
            name: "ForwardZil",
            args: []
        },
        {
            name: "WithUintConstructor",
            args: [123]
        },
        {
            name: "ParentContract",
            args: [],
            value: FUND
        }
    ]

    // Deploy Contracts in parallel
    let [forwardZil, withUint, parentContract] = await deployContracts(...contractsToDeploy);

    // First block
    await fillBlock(
        test("Balance of contract should be zero initially", 
            ethers.provider.getBalance(forwardZil.address),
            (obj) => expect(obj).to.be.eq(0)),

        test("If deposit is called, balance of the contract should be increased", 
            forwardZil.deposit({ value: FUND}),
            async () => expect(await ethers.provider.getBalance(forwardZil.address)).to.be.eq(FUND)),

        test("Balance of parent contract should be FUND initially", 
            ethers.provider.getBalance(parentContract.address),
            (obj) => expect(obj).to.be.eq(FUND)),

        test("WithUintConstructor should have 123", 
            withUint.number(),
            (value) => expect(value).to.be.eq(123))
    );
}

main()
    .then(() => process.exit(0))
    .catch((error) => {
        console.error(error);
        process.exit(1);
    });