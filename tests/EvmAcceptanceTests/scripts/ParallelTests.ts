import { ethers } from "hardhat";
import {ContractInfo, deployContracts} from "../helpers"
import {runScenarios} from "../helpers"
import {forwardZilScenario, moveZilScenario} from "../parallel-tests/Transfer"
import {withUintScenario} from "../parallel-tests/ContractDeployment"
import {parentScenario} from "../parallel-tests/Parent"
import {blockchainInstructionsScenario} from "../parallel-tests/BlockchainInstructions"
import {contractRevertScenario} from "../parallel-tests/ContractRevert"
import {create2Scenario} from "../parallel-tests/Create2"

import clc from "cli-color";
import { Chronometer, displayStageFinished, displayStageStarted } from "../helpers/parallel-tests/Display";

async function main() {
    const FUND = ethers.utils.parseUnits("1", "gwei");
    const NUMBER = 1234;

    const contractsToDeploy: ContractInfo[] = [
        {
            name: "ForwardZil",
        },
        {
            name: "ForwardZil",
        },
        {
            name: "WithUintConstructor",
            args: [NUMBER]
        },
        {
            name: "ParentContract",
            value: FUND
        },
        {
            name: "BlockchainInstructions",
        },
        {
            name: "Revert",
        },
        {
            name: "Create2Factory"
        }
    ]

    // Deploy Contracts in parallel
    let chronometer = new Chronometer();
    chronometer.start();
    displayStageStarted("Contracts are being deployed...");

    let [
            forwardZil,
            moveZilToContract, 
            withUint,
            parentContract,
            blockChainInstructionsContract,
            revertContract,
            create2Contract
        ] = await deployContracts(...contractsToDeploy);

    chronometer.finish();
    displayStageFinished(`${contractsToDeploy.length} contracts deployed`, chronometer);

    await runScenarios(
        withUintScenario(withUint, NUMBER),
        parentScenario(parentContract, FUND),
        await forwardZilScenario(forwardZil),
        await moveZilScenario(moveZilToContract),
        blockchainInstructionsScenario(blockChainInstructionsContract),
        // contractRevertScenario(revertContract),
        create2Scenario(create2Contract)
    );
}

main()
    .then(() => process.exit(0))
    .catch((error) => {
        console.error(error);
        process.exit(1);
    });