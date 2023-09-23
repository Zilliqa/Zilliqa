import {HardhatRuntimeEnvironment} from "hardhat/types";
import {Block, Scenario, TestInfo, Txn} from "./Scenario";
import fs from "fs";
import {getState, resetState} from "jest-circus";

const processDescribeChild = (
  child: any,
  describes: string[],
  regex: RegExp
): [tests: TestInfo[], beforeHooks: Txn[]] => {
  const nestedDescribes = [...describes, child.name];
  const allTests: TestInfo[] = [];
  const allBeforeBlocks: Txn[] = [];
  allBeforeBlocks.push(...child.hooks.filter((hook: any) => hook.type === "beforeAll").map((hook: any) => hook.fn));
  for (const subChild of child.children) {
    if (subChild.type === "describeBlock") {
      const [tests, beforeHooks] = processDescribeChild(subChild, nestedDescribes, regex);
      allTests.push(...tests);
      allBeforeBlocks.push(...beforeHooks);
    } else if (subChild.type === "test") {
      let block: Block;
      try {
        block = extractBlockNumber(subChild.name);
      } catch (error) {
        continue;
      }

      if (regex.test(subChild.name)) {
        allTests.push({
          txn: subChild.fn,
          msg: subChild.name,
          describes: nestedDescribes,
          run_in: block
        });
      }
    }
  }

  return [allTests, allBeforeBlocks];
};

export const parseTestFile = async function (
  testFile: string,
  regex: RegExp,
  hre: HardhatRuntimeEnvironment
): Promise<Scenario[]> {
  let scenarios: Scenario[] = [];

  let code = `
  const { describe, beforeAll, afterAll, it } = require("jest-circus");\n
  const before = beforeAll;\n
  const after = afterAll;\n
  const xit = () => {};
  ${await fs.promises.readFile(testFile, "utf8")}`;

  // Does file contain #parallel tag?? If not, skip it!
  if (!code.includes("#parallel")) {
    return [];
  }

  await fs.promises.writeFile(testFile, code);

  require(fs.realpathSync(testFile));

  const state = getState();

  for (const describeChild of state.rootDescribeBlock.children) {
    const describeName = describeChild.name;
    if (!isScenarioParallel(describeName)) {
      continue;
    }
    const [tests, beforeHooks] = processDescribeChild(describeChild, [], regex);

    if (regex.test(describeName) || tests.length > 0) {
      scenarios.push({
        scenario_name: describeName,
        beforeHooks,
        tests
      });
    }
  }

  resetState();
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
