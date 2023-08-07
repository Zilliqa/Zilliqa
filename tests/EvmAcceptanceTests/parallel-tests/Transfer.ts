import { ethers } from "hardhat";
import {Block, Scenario, scenario, test, xtest} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";
import { ForwardZil } from "../typechain-types";

export const forwardZilScenario = async function(contract: Contract): Promise<Scenario> {
    const FUND = ethers.utils.parseUnits("1", "gwei");

    let nonce = await contract.signer.getTransactionCount();

    return scenario("ForwardZil contract",
        test("Should return zero as the initial balance of the contract",
            async () => await ethers.provider.getBalance(contract.address),
            (result) => expect(result).to.be.eq(0),
            Block.BLOCK_1),
        
        test(`Should move ${ethers.utils.formatEther(FUND)} ethers to the contract if deposit is called`,
            async() => await contract.deposit({ value: FUND, nonce: nonce }),
            async () => expect(await ethers.provider.getBalance(contract.address)).to.be.eq(FUND),
            Block.BLOCK_1),

        test("Should move 1 ether to the owner if withdraw function is called so 1 ether is left for the contract itself [@transactional]",
            async () => await contract.withdraw({nonce: nonce + 1}),
            async (result) => {
                expect(result).to.changeEtherBalances(
                [contract.address, contract.address],
                [ethers.utils.parseEther("-1.0"), ethers.utils.parseEther("1.0")],
                {includeFee: true})
            },
            Block.BLOCK_1), 
    );
}

export const moveZilScenario = async function(contract: Contract): Promise<Scenario> {
    const FUND = ethers.utils.parseUnits("1", "gwei");

    const signer = contract.signer;
    let nonce = await contract.signer.getTransactionCount();
    const contractBalance = await ethers.provider.getBalance(contract.address);
    const payee = ethers.Wallet.createRandom();

    return scenario("Move zil scenario",
        test("should be possible to transfer ethers to the contract",
            async() => await signer.sendTransaction({
                to: contract.address,
                value: FUND,
                nonce: nonce
            }),
            async(_) => {
                const currentBalance = await ethers.provider.getBalance(contract.address);
                expect(currentBalance.sub(contractBalance)).to.be.eq(FUND);
            },
            Block.BLOCK_1
            ),
        test("should be possible to transfer ethers to a user account",
            async() => {
                await contract.signer.sendTransaction({
                    to: payee.address,
                    value: FUND,
                    nonce: nonce + 1
                });
                return ethers.provider.getBalance(payee.address)
            },
            (result) => expect(result).to.be.eq(FUND),
            Block.BLOCK_1)
    );
}