import clc from "cli-color";
import {performance} from "perf_hooks";
import {Chronometer, displayIgnored, displayStageFinished, displayStageStarted} from "./Display";
import { runStage } from "./Stage";

export type Txn = () => Promise<any>;

export enum Block {
  BLOCK_1,
  BLOCK_2,
  BLOCK_3
}

export type TransactionInfo = {
  txn: Txn;
  msg?: string;
  run_in: Block;
  disabled?: true;
};

export type Scenario = {
  scenario_name: string;
  before?: Txn;
  tests: TransactionInfo[];
};

export const scenario = function (scenario_name: string, ...tests: TransactionInfo[]): Scenario {
  return {
    scenario_name,
    tests
  };
};

export const test = function (msg: string, txn: Txn, run_in: Block): TransactionInfo {
  return {
    msg,
    txn,
    run_in
  };
};

export const xtest = function (msg: string, txn: Txn, run_in: Block): TransactionInfo {
  return {
    msg,
    txn,
    run_in,
    disabled: true
  };
};

export const xit = xtest;

export const transaction = function (txn: Txn, run_in: Block): TransactionInfo {
  return {
    txn,
    run_in
  };
};

export const it = test;

function sleep(ms: number) {
  return new Promise((resolve) => {
    setTimeout(resolve, ms);
  });
}

const execute = async function (txns: TransactionInfo[]) {
  let promises = [];
  for (let txn of txns) {
    if (txn.disabled) {
      if (txn.msg) displayIgnored(txn.msg);
      continue;
    }
    promises.push(txn.txn());
    await sleep(100);
  }

  await Promise.all(promises);
};

export const runScenarios = async function (...scenarios: Scenario[]) {
  let blocks = new Set(scenarios.flatMap((scenario) => scenario.tests.map((test) => test.run_in)));

  for (let block of blocks) {
    const txns = scenarios
        .map((scenario) => scenario.tests)
        .flat()
        .filter((scenario) => scenario.run_in == block)

    await runStage(`Running tests in block ${block}...`, () => {
      return execute(txns)
    }, () => {
      return `${txns.length} tests executed`
    }, );
  }
};
