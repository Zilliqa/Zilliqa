import { ethers } from "hardhat";
import {Block, Scenario, scenario, test} from "../helpers"
import { BigNumber, Contract } from "ethers";
import { expect } from "chai";

export const parentScenario = function(contract: Contract, initialFund: BigNumber): Scenario {
    return scenario("Parent contract",
        test("Balance of parent contract should be FUND initially", 
            ethers.provider.getBalance(contract.address),
            (obj) => expect(obj).to.be.eq(initialFund),
            Block.BLOCK_1
    ));
}