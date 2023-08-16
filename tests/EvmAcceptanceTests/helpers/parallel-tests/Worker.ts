import { HardhatRuntimeEnvironment } from "hardhat/types";
import {Block, Scenario, TransactionInfo, Txn} from "./Scenario";
import fs from "fs";

export const parseTestFile = async function (testFile: string, regex: RegExp, hre: HardhatRuntimeEnvironment): Promise<Scenario[]> {
  let scenarios: Scenario[] = [];

  let code = await fs.promises.readFile(testFile, "utf8");

  const testResult = {
    success: false,
    errorMessage: null
  };

  try {
    const describeFns: [string, Txn][] = [];
    let currentDescribeFn: [string, Txn][];
    let currentBeforeFn;
    let currentAfterFn;
    const describe = (name: string, fn: Txn) => describeFns.push([name, fn]);
    const it = (name: string, fn: Txn) => currentDescribeFn.push([name, fn]);
    const xit = (name: string, fn: Txn) => {};
    const before = (fn: Txn) => (currentBeforeFn = fn);
    const after = (fn: Txn) => (currentAfterFn = fn);

    try {
      eval(code);
    } catch (error) {
      if (hre.debug) {
        console.log(error);
      }
      return [];
    }
    for (const [describeName, fn] of describeFns) {
      if (!isScenarioParallel(describeName)) {
        continue;
      }

      const describeMatched = regex.test(describeName);

      let transaction_infos: TransactionInfo[] = [];
      currentDescribeFn = [];
      currentBeforeFn = undefined;
      currentAfterFn = undefined;
      fn();

      for (const [name, fn] of currentDescribeFn) {
        let block: Block;
        const testMatched = regex.test(name);
        try {
          block = extractBlockNumber(name);
        } catch (error) {
          continue;
        }

        if (describeMatched || testMatched) {
          transaction_infos.push({
            txn: fn,
            scenario_name: describeName,
            msg: name,
            run_in: block
          });
        }
      }

      if (describeMatched || transaction_infos.length > 0) {    // Added describeMatched to catch forgotten @block-n in test descs
        scenarios.push({
          before: currentBeforeFn,
          after: currentAfterFn,
          scenario_name: describeName,
          tests: transaction_infos
        });
      }
    }
    testResult.success = true;
  } catch (error) {
    console.log("error", error);
  }

  return scenarios;
};

const extractBlockNumber = (description: string): number => {
  const match = description.match(/@block-(\d+)/);
  if (!match) {
    throw new Error(`Invalid description for test \`${description}\`. It should contain block number like @block-12`);
  }
  return Number(match[1]) - 1;
};

const isScenarioParallel = (description: string): boolean => {
  return /#parallel/.test(description);
};
