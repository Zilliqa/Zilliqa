import {FailureResult, parseTestFile, runStage} from "../helpers/parallel-tests";
import {Scenario, runScenarios} from "../helpers/parallel-tests";
import {displayIgnored} from "../helpers/parallel-tests";
import fs from "fs";
import path from "path";
import clc from "cli-color";
import util from "util";
import {exec} from "child_process";
import {subtask, task} from "hardhat/config";
const execa = util.promisify(exec);

task("parallel-test", "Runs test in parallel")
  .addOptionalParam("grep", "Only run tests matching the given string or regexp")
  .setAction(async (taskArgs, hre) => {
    hre.run("compile");
    const [eth_signer_count, zil_signer_count] = hre.signer_pool.count();

    const {grep, testFiles}: {grep: string | undefined; testFiles: string[]} = taskArgs;

    // Running Typescript
    await hre.run("parallel-test:run-tsc");

    // Getting all files to tests
    const filesToTest = await hre.run("parallel-test:get-files", {testFiles});

    // Parsing files to extract parallel scenarios
    const [beforeFns, scenarios]: [Promise<any>[], Scenario[]] = await hre.run(
      "parallel-test:parse-files",
      {testFiles: filesToTest, grep},
      hre
    );

    if (scenarios.length === 0) {
      displayIgnored("No scenarios found or your --grep doesn't have any matches.");
      return;
    }

    // Deploying contracts concurrently
    await hre.run("parallel-test:deploy", {beforeFns});

    // Running tests
    const failures: FailureResult[] = await hre.run("parallel-test:run", {scenarios});

    // Display results
    await hre.run("parallel-test:output-results", {failures});

    const [current_eth_signer_count, current_zil_signer_count] = hre.signer_pool.count();
    console.log(`🪪  ${clc.bold.white(eth_signer_count - current_eth_signer_count)} eth signers used.`);
    console.log(`🪪  ${clc.bold.white(zil_signer_count - current_zil_signer_count)} zil signers used.`);
  });

subtask("parallel-test:run-tsc", "Runs tsc to make sure everything's synced").setAction(async () => {
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
});

subtask("parallel-test:get-files", "Gets a list of files to be tested")
  .addOptionalVariadicPositionalParam("testFiles", "An optional list of files to test", [])
  .setAction(async ({testFiles}: {testFiles: string[]}): Promise<string[]> => {
    const walk = function (dir: string): string[] {
      return fs
        .readdirSync(dir, {withFileTypes: true})
        .flatMap((file) => (file.isDirectory() ? walk(path.join(dir, file.name)) : path.join(dir, file.name)));
    };

    const userSpecifiedTestFiles = testFiles.map((file) => {
      const parsed = path.parse(file);
      return path.format({...parsed, base: "", dir: path.join("dist", parsed.dir), ext: ".js"});
    });

    const filesToTest = userSpecifiedTestFiles.length === 0 ? ["dist/test"] : userSpecifiedTestFiles;

    return filesToTest.flatMap((filename) => {
      if (fs.lstatSync(filename).isDirectory()) {
        return walk(filename);
      }
      return [filename];
    });
  });

subtask("parallel-test:parse-files", "Parses files to extract parallel tests")
  .addOptionalVariadicPositionalParam("testFiles", "An optional list of files to test", [])
  .addOptionalParam("grep", "Only run tests matching the given string or regexp", "")
  .setAction(
    async ({testFiles, grep}: {testFiles: string[]; grep: string}, hre): Promise<[Promise<any>[], Scenario[]]> => {
      const regex = new RegExp(grep);
      return runStage(
        "Analyzing tests to run...",
        async (files: string[]) => {
          let beforeFns: Promise<any>[] = [];
          let scenarios: Scenario[] = [];

          for (const test_file of files) {
            const parsed = await parseTestFile(test_file, regex, hre);
            parsed.forEach((scenario) => {
              if (scenario.tests.length === 0) {
                displayIgnored(`\`${scenario.scenario_name}\` doesn't have any tests. Did you add @block-n to tests?`);
                return;
              }

              beforeFns.push(...scenario.beforeHooks.map((hook) => hook()));
              scenarios.push(scenario);
            });
          }

          return [beforeFns, scenarios];
        },
        (params: string[], output: any) => {
          return {
            finished_message: `Found ${output[1].length} scenarios to run`,
            success: true
          };
        },
        testFiles
      );
    }
  );

subtask("parallel-test:deploy", "Deploys contracts in parallel").setAction(async (taskArgs) => {
  const {beforeFns} = taskArgs;

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
});

subtask("parallel-test:run", "Runs the tests").setAction(async (taskArgs) => {
  const {scenarios}: {scenarios: Scenario[]} = taskArgs;
  return runStage(
    "Running test Results",
    (scenarios: Scenario[]) => {
      return runScenarios(...scenarios);
    },
    (params: any, output: PromiseSettledResult<any>[]) => {
      const tests_count = scenarios.map((scenario) => scenario.tests.length).reduce((prev, current) => prev + current);
      const failedCount: number = output.length;
      return {
        finished_message:
          `${scenarios.length} scenarios and ${tests_count} tests executed. ` +
          `${clc.bold.greenBright(tests_count - failedCount)} ${clc.green("passed")}` +
          (failedCount > 0 ? `, ${clc.bold.redBright(failedCount)} ${clc.red("failed")}!` : ""),
        success: failedCount === 0 ? true : false
      };
    },
    scenarios
  );
});

subtask("parallel-test:output-results", "Outputs test results").setAction(async (taskArgs) => {
  const {failures}: {failures: FailureResult[]} = taskArgs;
  const space = (n: number) => " ".repeat(n);

  if (failures.length > 0) {
    console.log(clc.bold.bgRed(`Failures (${failures.length})`));
    failures.forEach((failure, index) => {
      console.log(clc.bold.white(`${index + 1})`));
      let currentIndent = 1;
      failure.describes.forEach((describe, describeIndex) => {
        currentIndent += describeIndex;
        console.log(`${space(currentIndent)}${describe}`);
      });
      console.log(`${space(currentIndent + 1)}${clc.white.bold(failure.test_case)}`);
      console.log(
        `${space(currentIndent + 2)}${clc.red.bold("Actual: ")} ${clc.red((failure.result as any).reason.actual)}`
      );
      console.log(
        `${space(currentIndent + 2)}${clc.green.bold("Expected: ")} ${clc.red((failure.result as any).reason.expected)}`
      );
      console.log(
        `${space(currentIndent + 2)}${clc.yellow.bold("Operator: ")} ${clc.red(
          (failure.result as any).reason.operator
        )}`
      );
      console.log();
    });
  }
});
