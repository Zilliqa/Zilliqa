import clc from "cli-color";
import {displayIgnored} from "./Display";
import {runStage} from "./Stage";

export type Txn = () => Promise<any>;

export enum Block {
  BLOCK_1,
  BLOCK_2,
  BLOCK_3
}

export type TestInfo = {
  txn: Txn;
  msg: string;
  describes: string[]; // Because `describe` blocks can be nested, this test can belong to a nested describe, this is a list of all of its describes
  run_in: Block;
  disabled?: true;
};

export type Scenario = {
  scenario_name: string;
  beforeHooks: Txn[];
  tests: TestInfo[];
  after?: Txn;
};

export type FailureResult = {
  result: PromiseSettledResult<any>;
  test_case: string;
  describes: string[];
};

const execute = async function (txns: TestInfo[]): Promise<FailureResult[]> {
  let promises = [];
  for (let txn of txns) {
    if (txn.disabled) {
      if (txn.msg) displayIgnored(txn.msg);
      continue;
    }
    promises.push(txn.txn());
  }

  const failures: FailureResult[] = [];
  const results = await Promise.allSettled(promises);

  results.forEach((result, index) => {
    if (result.status == "rejected") {
      failures.push({
        result,
        test_case: txns[index].msg,
        describes: txns[index].describes
      });
    }
  });

  return failures;
};

export const runScenarios = async function (...scenarios: Scenario[]): Promise<FailureResult[]> {
  let blocks = new Set(scenarios.flatMap((scenario) => scenario.tests.map((test) => test.run_in)));

  const failures: FailureResult[] = [];

  for (let block of blocks) {
    const txns = scenarios
      .map((scenario) => scenario.tests)
      .flat()
      .filter((scenario) => scenario.run_in == block);

    await runStage(
      `Running tests in block ${block + 1}...`,
      () => {
        return execute(txns);
      },
      (params: any, output: FailureResult[]) => {
        failures.push(...output);
        const failedCount: number = output.length;
        return {
          finished_message:
            `${txns.length} tests executed` +
            (failedCount > 0 ? `, ${clc.bold.redBright(failedCount)} ${clc.red("failed")}!` : ""),
          success: failedCount === 0 ? true : false
        };
      }
    );
  }

  return failures;
};
