import clc from "cli-color";
import {displayIgnored} from "./Display";
import {runStage} from "./Stage";

export type Txn = () => Promise<any>;

export enum Block {
  BLOCK_1,
  BLOCK_2,
  BLOCK_3
}

<<<<<<< HEAD
export type TestInfo = {
  txn: Txn;
  msg: string;
  describes: string[];   // Because `describe` blocks can be nested, this test can belong to a nested describe, this is a list of all of its describes
=======
export type TransactionInfo = {
  txn: Txn;
  msg: string;
  scenario_name: string;
>>>>>>> 945caad7b ([q4-working-branch] Q4 network plus (#3743))
  run_in: Block;
  disabled?: true;
};

export type Scenario = {
  scenario_name: string;
<<<<<<< HEAD
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
=======
  before?: Txn;
  tests: TransactionInfo[];
};

export const scenario = function (scenario_name: string, ...tests: TransactionInfo[]): Scenario {
  return {
    scenario_name,
    tests
  };
};

export type FailureResult = {
  result: PromiseSettledResult<any>,
  test_case: string,
  scenario: string,
}

const execute = async function (txns: TransactionInfo[]): Promise<FailureResult[]> {
>>>>>>> 945caad7b ([q4-working-branch] Q4 network plus (#3743))
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
<<<<<<< HEAD
        describes: txns[index].describes
      });
    }
  });
=======
        scenario: txns[index].scenario_name
      })
    }
  })
>>>>>>> 945caad7b ([q4-working-branch] Q4 network plus (#3743))

  return failures;
};

export const runScenarios = async function (...scenarios: Scenario[]): Promise<FailureResult[]> {
  let blocks = new Set(scenarios.flatMap((scenario) => scenario.tests.map((test) => test.run_in)));

  const failures: FailureResult[] = [];

  for (let block of blocks) {
    const txns = scenarios
<<<<<<< HEAD
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
=======
        .map((scenario) => scenario.tests)
        .flat()
        .filter((scenario) => scenario.run_in == block)

    await runStage(`Running tests in block ${block}...`, () => {
      return execute(txns)
    }, (params: any, output: FailureResult[]) => {
      failures.push(...output)
      const failedCount: number = output.length;
      return {
        finished_message: `${txns.length} tests executed` + (failedCount > 0 ? `, ${clc.bold.redBright(failedCount)} ${clc.red("failed")}!` : ""),
        success: failedCount === 0 ? true : false
      }
    }, );
>>>>>>> 945caad7b ([q4-working-branch] Q4 network plus (#3743))
  }

  return failures;
};
