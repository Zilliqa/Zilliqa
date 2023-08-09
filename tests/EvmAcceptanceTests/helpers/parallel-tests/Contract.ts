import { BigNumber, Contract } from "ethers";
import { ethers } from "hardhat";
import SignerPool from "../SignerPool";

export type ContractInfo = {
    name: string;
    args?: any[];
    value?: BigNumber
};

export const deployContracts = async function (...contracts: ContractInfo[]) {
    let signer_pool = new SignerPool();
    await signer_pool.initSigners(contracts.length, 0.1);
    let contractsPromises: Promise<Contract>[] = [];
    for (const index in contracts) {
        const contract = contracts[index];
        const signer = await signer_pool.takeSigner();
        const Contract = await ethers.getContractFactory(contract.name);
        let args = contract.args || [];
        contractsPromises.push(Contract.connect(signer).deploy(...args, { value: contract.value }));
    }

    return Promise.all(contractsPromises);
}