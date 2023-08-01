import { ethers } from "hardhat";
import {ContractInfo, deployContracts} from "../helpers"
import {runScenarios} from "../helpers"
import {forwardZilScenario} from "../parallel-tests/Transfer"
import {withUintScenario} from "../parallel-tests/ContractDeployment"
import {parentScenario} from "../parallel-tests/Parent"

async function main() {
    const FUND = ethers.utils.parseUnits("1", "gwei");
    const NUMBER = 1234;

    const contractsToDeploy: ContractInfo[] = [
        {
            name: "ForwardZil",
            args: []
        },
        {
            name: "WithUintConstructor",
            args: [NUMBER]
        },
        {
            name: "ParentContract",
            args: [],
            value: FUND
        }
    ]

    // Deploy Contracts in parallel
    let [forwardZil, withUint, parentContract] = await deployContracts(...contractsToDeploy);

    await runScenarios(
        withUintScenario(withUint, NUMBER),
        parentScenario(parentContract, FUND),
        forwardZilScenario(forwardZil),
    );
}

main()
    .then(() => process.exit(0))
    .catch((error) => {
        console.error(error);
        process.exit(1);
    });