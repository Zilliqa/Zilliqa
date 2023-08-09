
import {Block, Scenario, scenario, test} from "../helpers"
import { Contract } from "ethers";
import { expect } from "chai";

export const blockchainInstructionsScenario = function(contract: Contract): Scenario {
    const owner = contract.signer;
    return scenario("Blockchain instructions",
        test("Should return the owner address when getOrigin function is called", async() => {
              expect(await contract.getOrigin()).to.be.eq(await owner.getAddress())
            },
            Block.BLOCK_1
        ));
}