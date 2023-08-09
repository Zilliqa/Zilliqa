import {Block, Scenario, Txn, scenario, test} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";

export const withUintScenario = function(contract: Contract, number: Number): Scenario {
    return scenario("With unit constructor",
        test("WithUintConstructor should have 123", async() => {
                expect(await contract.number()).to.be.eq(number);
            },
            Block.BLOCK_1
        ));
}