import {Block, Scenario, TransactionInfo, Txn} from "./Scenario";

import fs from "fs";

export const parseTestFile = async function (testFile: string): Promise<Scenario[]> {
  let scenarios: Scenario[] = [];

  let code = await fs.promises.readFile(testFile, "utf8");
  // code = ts.transpileModule(code, { compilerOptions:
  //     {
  //         forceConsistentCasingInFileNames: true,
  //         module: ts.ModuleKind.CommonJS,
  //         target: ts.ScriptTarget.ES2020,
  //         esModuleInterop: true,
  //         skipLibCheck: true,
  //         moduleResolution: ts.ModuleResolutionKind.Node16,
  //         strict: true,
  //     } }).outputText;

  const testResult = {
    success: false,
    errorMessage: null
  };

  try {
    const describeFns: [string, Txn][] = [];
    let currentDescribeFn: [string, Txn][];
    let currentBeforeFn;
    const describe = (name: string, fn: Txn) => describeFns.push([name, fn]);
    const it = (name: string, fn: Txn) => currentDescribeFn.push([name, fn]);
    const xit = (name: string, fn: Txn) => {};
    const before = (fn: Txn) => (currentBeforeFn = fn);

    eval(code);
    for (const [name, fn] of describeFns) {
      if (!isScenarioParallel(name)) {
        continue;
      }

      let transaction_infos: TransactionInfo[] = [];
      currentDescribeFn = [];
      currentBeforeFn = undefined;
      fn();

      for (const [name, fn] of currentDescribeFn) {
        let block: Block;
        try {
          block = extractBlockNumber(name);
        } catch (error) {
          continue;
        }

        transaction_infos.push({
          txn: fn,
          msg: name,
          run_in: block
        });
      }

      scenarios.push({
        before: currentBeforeFn,
        scenario_name: name,
        tests: transaction_infos
      });
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
