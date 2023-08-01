import { BigNumber, Contract } from "ethers";
import { ethers } from "hardhat";

export type ContractInfo = {
    name: string;
    args: any[];
    value?: BigNumber
};

export const deployContracts = async function (...contracts: ContractInfo[]) {
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