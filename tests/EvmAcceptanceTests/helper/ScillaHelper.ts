import {Zilliqa} from "@zilliqa-js/zilliqa";
import fs from "fs";
import {BN, Long, units, bytes} from "@zilliqa-js/util";
import {Init, Contract, Value} from "@zilliqa-js/contract";
import {ContractInfo} from "./ScillaContractsInfoUpdater";
import {TransitionParam, isNumeric, Fields, ContractName} from "./ScillaParser";
import hre from "hardhat";

interface Setup {
  zilliqa: Zilliqa;
  readonly attempts: number;
  readonly timeout: number;
  readonly version: number;
  readonly gasPrice: BN;
  readonly gasLimit: Long;
}

const init = (): Setup => {
  const s: Setup = {
    zilliqa: new Zilliqa(hre.getNetworkUrl()),
    version: bytes.pack(hre.getZilliqaChainId(), 1),
    gasPrice: units.toQa("2000", units.Units.Li),
    gasLimit: Long.fromNumber(50000),
    attempts: 10,
    timeout: 1000
  };

  s.zilliqa.wallet.addByPrivateKey("254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004");
  return s;
};

const setup = init();

function read(f: string) {
  let t = fs.readFileSync(f, "utf8");
  return t;
}

export type ContractFunction<T = any> = (...args: Array<any>) => Promise<T>;

export class ScillaContract extends Contract {
  // Transitions and fields
  [key: string]: ContractFunction | any;
}

export async function deploy(contractName: string, ...args: any[]) {
  let contractInfo: ContractInfo = hre.scillaContracts[contractName];
  if (contractInfo === undefined) {
    throw new Error(`Scilla contract ${contractName} doesn't exist.`);
  }

  let sc: ScillaContract;
  let init: Init = fillInit(contractName, contractInfo.parsedContract.constructorParams, ...args);

  sc = await deploy_from_file(contractInfo.path, init);
  contractInfo.parsedContract.transitions.forEach((transition) => {
    sc[transition.name] = async (...args: any[]) => {
      if (args.length !== transition.params.length) {
        throw new Error(
          `Expected to receive ${transition.params.length} parameters for ${transition.name} but got ${args.length}`
        );
      }

      const values: Value[] = [];
      transition.params.forEach((param: TransitionParam, index: number) => {
        values.push({
          vname: param.name,
          type: param.type,
          value: args[index].toString()
        });
      });

      return sc_call(sc, transition.name, values);
    };

    contractInfo.parsedContract.fields.forEach((field) => {
      sc[field.name] = async () => {
        const state = await sc.getState();
        if (isNumeric(field.type)) return Number(state[field.name]);
        return state[field.name];
      };
    });
  });

  return sc;
}

const fillInit = (contractName: string, contractParams: Fields | null, ...userSpecifiedArgs: any[]): Init => {
  let init: Init = [{vname: "_scilla_version", type: "Uint32", value: "0"}];

  if (contractParams) {
    if (userSpecifiedArgs.length !== contractParams.length) {
      throw new Error(
        `Expected to receive ${contractParams.length} parameters for ${contractName} deployment but got ${userSpecifiedArgs.length}`
      );
    }
    contractParams.forEach((param: TransitionParam, index: number) => {
      init.push({
        vname: param.name,
        type: param.type,
        value: userSpecifiedArgs[index].toString()
      });
    });
  } else {
    if (userSpecifiedArgs.length > 0) {
      throw new Error(
        `Expected to receive 0 parameters for ${contractName} deployment but got ${userSpecifiedArgs.length}`
      );
    }
  }

  return init;
};

// deploy a smart contract whose code is in a file with given init arguments
async function deploy_from_file(path: string, init: Init): Promise<ScillaContract> {
  const code = read(path);
  const contract = setup.zilliqa.contracts.new(code, init);
  let [_, sc] = await contract.deploy({...setup}, setup.attempts, setup.timeout, false);

  return sc;
}

// call a smart contract's transition with given args and an amount to send from a given public key
export async function sc_call(
  sc: Contract,
  transition: string,
  args: Value[] = [],
  amt = new BN(0)
  // caller_pub_key = setup.pub_keys[0]
) {
  return sc.call(
    transition,
    args,
    {
      version: setup.version,
      amount: amt,
      gasPrice: setup.gasPrice,
      gasLimit: setup.gasLimit
      // pubKey: caller_pub_key
    },
    setup.attempts,
    setup.timeout,
    true
  );
}
