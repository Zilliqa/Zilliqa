import {FailureResult, parseTestFile, runStage} from "../helpers";
import {Scenario, runScenarios} from "../helpers";
import {displayIgnored} from "../helpers";
import hre, {ethers} from "hardhat";
import fs from "fs";
import path from "path";
import clc from "cli-color";
import util from "util";
import {exec} from "child_process";
const execa = util.promisify(exec);

const PARALYZED_TEST_FILES: string[] = [
  "./dist/test/BlockchainInstructions.js",
  "./dist/test/Create2.js",
  "./dist/test/Delegatecall.js",
  "./dist/test/Errorneous.js", // Takes 13 seconds to finish
  "./dist/test/precompiles/EvmPrecompiles.js",
  "./dist/test/rpc/"

  // "./dist/test/Transfer.js"
  // "./dist/test/ContractRevert.js",
];

const filesToTest = (): string[] => {
  return PARALYZED_TEST_FILES.flatMap((filename) => {
    if (fs.lstatSync(filename).isDirectory()) {
      return fs
        .readdirSync(filename, {withFileTypes: true})
        .filter((item) => !item.isDirectory())
        .map((item) => path.join(filename, item.name));
    }
    return [filename];
  });
};

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
};

async function main() {
  hre.signer_pool.initSigners(...(await ethers.getSigners()));

  await runStage(
    "Running tsc...",
    async () => {
      try {
        await execa("tsc");
        return "Success";
      } catch (error: any) {
        return `Failed (${error.stdout.split("\n").length} errors)`;
      }
    },
    (params: any, output: string) => {
      return {
        finished_message: output,
        success: output === "Success" ? true : false
      };
    }
  );

  const [beforeFns, scenarios]: [Promise<any>[], Scenario[]] = await runStage(
    "Analyzing tests to run...",
    (files: string[]) => {
      return parseFiles(files);
    },
    (params: string[], output: any) => {
      return {
        finished_message: `Found ${output[1].length} scenarios to run`,
        success: true
      };
    },
    filesToTest()
  );

  await runStage(
    "Deploying contracts",
    (beforeFns: Promise<any>[]) => {
      return Promise.all(beforeFns);
    },
    () => {
      return {
        finished_message: `${beforeFns.length} contracts deployed`,
        success: true
      };
    },
    beforeFns
  );

  const failures: FailureResult[] = await runStage(
    "Running tests",
    (scenarios: Scenario[]) => {
      return runScenarios(...scenarios);
    },
    (params: any, output: PromiseSettledResult<any>[]) => {
      const tests_count = scenarios.map((scenario) => scenario.tests.length).reduce((prev, current) => prev + current);
      const failedCount: number = output.length;
      return {
        finished_message:
          `${scenarios.length} scenario and ${tests_count} tests executed` +
          (failedCount > 0 ? `, ${clc.bold.redBright(failedCount)} ${clc.red("failed")}!` : ""),
        success: failedCount === 0 ? true : false
      };
    },
    scenarios
  );

  if (failures.length > 0) {
    console.log(clc.bold.bgRed(`Failures (${failures.length})`));
    failures.forEach((failure, index) => {
      console.log(` ${clc.bold.white(index + 1)}) ${failure.scenario}`);
      console.log(`  ${clc.red("✖")} ${clc.blackBright(failure.test_case)}`);
      console.log(`    ${clc.red.bold("Actual: ")} ${clc.red((failure.result as any).reason.actual)}`);
      console.log(`    ${clc.green.bold("Expected: ")} ${clc.red((failure.result as any).reason.expected)}`);
      console.log(`    ${clc.yellow.bold("Operator: ")} ${clc.red((failure.result as any).reason.operator)}`);
      console.log();
    });
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
``;