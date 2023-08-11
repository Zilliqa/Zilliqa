import {parseTestFile} from "../helpers";
import {Scenario, runScenarios} from "../helpers";
import {displayIgnored} from "../helpers";
import hre, {ethers} from "hardhat";
import fs from "fs";
import path from "path";

const PARALYZED_TEST_FILES: string[] = [
  "./dist/test/BlockchainInstructions.js",
  "./dist/test/Create2.js",
  //   "./dist/test/Errorneous.js",
  "./dist/test/Delegatecall.js",
  // // "./dist/test/ContractRevert.js",
  "./dist/test/precompiles/EvmPrecompiles.js",
  "./dist/test/rpc/"
];

async function main() {
  hre.signer_pool.initSigners(...(await ethers.getSigners()));

  let beforeFns: Promise<any>[] = [];
  let scenarios: Scenario[] = [];

  const files = PARALYZED_TEST_FILES.flatMap((filename) => {
    if (fs.lstatSync(filename).isDirectory()) {
      return fs
        .readdirSync(filename, {withFileTypes: true})
        .filter((item) => !item.isDirectory())
        .map((item) => path.join(filename, item.name));
    }
    return [filename];
  });

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

  if (beforeFns.length > 0) {
    await Promise.all(beforeFns);
  }

  if (scenarios.length > 0) {
    await runScenarios(...scenarios);
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
``;
