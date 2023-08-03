import clc from "cli-color";
import { performance } from "perf_hooks";

export type Assertion = (value: any) => any;
export type Txn = () => Promise<any>;

export enum Block {
    BLOCK_1,
    BLOCK_2,
    BLOCK_3,
}

export type TransactionInfo = {
    txn: Txn;
    assertion?: Assertion;
    msg?: string;
    run_in: Block;
    disabled?: true;
}

export type Scenario = {
    scenario_name: string;
    tests: TransactionInfo[];
}

export const scenario = function (scenario_name: string, ...tests: TransactionInfo[]): Scenario {
    return {
        scenario_name,
        tests
    };
}

export const test = function (msg: string, txn: Txn, assertion: Assertion, run_in: Block): TransactionInfo {
    return {
        msg,
        txn,
        assertion,
        run_in
    }
}

export const xtest = function (msg: string, txn: Txn, assertion: Assertion, run_in: Block): TransactionInfo {
    return {
        msg,
        txn,
        assertion,
        run_in,
        disabled: true
    }
}

export const transaction = function (txn: Txn, run_in: Block): TransactionInfo {
    return {
        txn,
        run_in
    }
}

export const it = test;

const execute = async function (txns: TransactionInfo[]) {
    const values = await Promise.all(txns.map((txnInfo) => txnInfo.txn()));
    for (const index in txns) {
        let txn = txns[index];
        if (txn.disabled) {
            console.log(clc.cyanBright(`- ${txn.msg}`));
            continue;
        }

        if (txn.assertion) {

            try {
                await txn.assertion(values[index]);
                console.log(`${clc.green('✔')} ${clc.blackBright(txn.msg)}`);
            } catch (error) {
                console.log(`❌ ${clc.bold(txn.msg)}`);
            }
        }
    }
}

export const runScenarios = async function (...scenarios: Scenario[]) {
    const start = performance.now();
    console.log(clc.bold(`Running tests in block ${Block.BLOCK_1}...`));
    await execute(scenarios
            .map((scenario) => scenario.tests)
            .flat()
            .filter((scenario) => scenario.run_in == Block.BLOCK_1));
    const end = performance.now();
    console.log(clc.blackBright("  done "), clc.bold.green(`${((end - start) / 1000).toPrecision(2)} s`));

    // console.log(clc.bold(`Running tests in block ${Block.BLOCK_2}...`));
    // await execute(scenarios
    //         .map((scenario) => scenario.tests)
    //         .flat()
    //         .filter((scenario) => scenario.run_in == Block.BLOCK_2));

    // console.log(clc.bold(`Running tests in block ${Block.BLOCK_3}...`));
    // await execute(scenarios
    //         .map((scenario) => scenario.tests)
    //         .flat()
    //         .filter((scenario) => scenario.run_in == Block.BLOCK_3));
}