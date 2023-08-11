import { Chronometer, displayStageFinished, displayStageStarted, parseTestFile, runStage } from "../helpers";
import { Scenario, runScenarios } from "../helpers";
import { displayIgnored } from "../helpers";
import hre, { ethers } from "hardhat";
import fs from "fs";
import path from "path";
import ora from "ora";


const PARALYZED_TEST_FILES: string[] = [
  "./dist/test/BlockchainInstructions.js",
  "./dist/test/Create2.js",
  //   "./dist/test/Errorneous.js",
  "./dist/test/Delegatecall.js",
  // // "./dist/test/ContractRevert.js",
  "./dist/test/precompiles/EvmPrecompiles.js",
  "./dist/test/rpc/"
];

const filesToTest = (): string[] => {
  return PARALYZED_TEST_FILES.flatMap((filename) => {
    if (fs.lstatSync(filename).isDirectory()) {
      return fs
        .readdirSync(filename, { withFileTypes: true })
        .filter((item) => !item.isDirectory())
        .map((item) => path.join(filename, item.name));
    }
    return [filename];
  });
}

const parseFiles = async (files: string[]): Promise<[Promise<any>[], Scenario[]]> => {
  let beforeFns: Promise<any>[] = [];
  let scenarios: Scenario[] = [];
  
  for (const test_file of files) {
    const parsed = await parseTestFile(test_file);
    parsed.forEach((scenario) => {
      if (scenario.tests.length === 0) {
        displayIgnored(`\`${scenario.scenario_name}\` doesn't have any tests. Did you add @block-n to tests?`);
        return;
      }
      if (scenario.before) {
        beforeFns.push(scenario.before());
      }
      scenarios.push(scenario);
    });
  }

  return [beforeFns, scenarios];
}

async function main() {
  hre.signer_pool.initSigners(...(await ethers.getSigners()));

  const [beforeFns, scenarios]: [Promise<any>[], Scenario[]] = await runStage("Analyzing tests to run...", 
    (files: string[]) => {
      return parseFiles(files);
    },
    (params: string[], output: any) => {
      return `Found ${output[1].length} scenarios to run`;
    },
     filesToTest())

  await runStage("Deploying contracts", (beforeFns: Promise<any>[]) => {
    return Promise.all(beforeFns);
  }, () => {
    return `${beforeFns.length} contracts deployed`
  }, beforeFns)

  await runStage("Running tests", (scenarios: Scenario[]) => {
    return runScenarios(...scenarios);
  }, () => {
    const tests_count = scenarios.map(scenario => scenario.tests.length).reduce((prev, current) => prev + current);
    return `${scenarios.length} scenarios and ${tests_count} tests executed`
  }, scenarios)
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
``;
