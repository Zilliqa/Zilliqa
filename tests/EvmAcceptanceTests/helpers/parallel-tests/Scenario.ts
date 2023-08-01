import clc from "cli-color";

export type Assertion = (value: any) => any;

export enum Block {
    BLOCK_1,
    BLOCK_2,
    BLOCK_3,
}

export type TransactionInfo = {
    txn: Promise<any>;
    assertion?: Assertion;
    msg?: string;
    run_in: Block;
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

export const test = function (msg: string, txn: Promise<any>, assertion: Assertion, run_in: Block): TransactionInfo {
    return {
        msg,
        txn,
        assertion,
        run_in
    }
}

export const it = test;

const execute = async function (txns: TransactionInfo[]) {
    const values = await Promise.all(txns.map((txnInfo) => txnInfo.txn));
    for (const index in txns) {
        let txn = txns[index];
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
    for (const block in Block) {
        await execute(scenarios
                .map((scenario) => scenario.tests)
                .flat()
                .filter((scenario) => scenario.run_in == Block[block as keyof typeof Block]));
    }
}