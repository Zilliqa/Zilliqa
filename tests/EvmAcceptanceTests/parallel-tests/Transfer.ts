import { ethers } from "hardhat";
import {Block, Scenario, scenario, test} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";

export const forwardZilScenario = function(contract: Contract): Scenario {
    const FUND = ethers.utils.parseUnits("1", "gwei");

    return scenario("ForwardZil contract",
        test("Balance of contract should be zero initially",
            ethers.provider.getBalance(contract.address),
            (obj) => expect(obj).to.be.eq(0),
            Block.BLOCK_1),

        test("If deposit is called, balance of the contract should be increased",
            contract.deposit({ value: FUND }),
            async () => expect(await ethers.provider.getBalance(contract.address)).to.be.eq(FUND),
            Block.BLOCK_1)
    );
}