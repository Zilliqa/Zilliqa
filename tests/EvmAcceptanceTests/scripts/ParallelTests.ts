import { ethers } from "hardhat";
import {ContractInfo, deployContracts} from "../helpers"
import {runScenarios} from "../helpers"
import {forwardZilScenario, moveZilScenario} from "../parallel-tests/Transfer"
import {withUintScenario} from "../parallel-tests/ContractDeployment"
import {parentScenario} from "../parallel-tests/Parent"
import clc from "cli-color";
import { performance } from "perf_hooks";

async function main() {
    const FUND = ethers.utils.parseUnits("1", "gwei");
    const NUMBER = 1234;

    const contractsToDeploy: ContractInfo[] = [
        {
            name: "ForwardZil",
            args: []
        },
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
    const start = performance.now();
    console.log(clc.bold("Contracts are being deployed..."));
    let [forwardZil, moveZilToContract, withUint, parentContract] = await deployContracts(...contractsToDeploy);
    const end = performance.now();
    console.log(clc.blackBright("  done "), clc.bold.green(`${((end - start) / 1000).toPrecision(2)} s`));

    await runScenarios(
        withUintScenario(withUint, NUMBER),
        parentScenario(parentContract, FUND),
        await forwardZilScenario(forwardZil),
        await moveZilScenario(moveZilToContract),
    );
}

main()
    .then(() => process.exit(0))
    .catch((error) => {
        console.error(error);
        process.exit(1);
    });