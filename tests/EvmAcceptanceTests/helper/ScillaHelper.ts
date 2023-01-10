import {Zilliqa} from "@zilliqa-js/zilliqa";
import fs from "fs";
import {BN, Long, units, bytes} from "@zilliqa-js/util";
import {getAddressFromPrivateKey, getPubKeyFromPrivateKey} from "@zilliqa-js/crypto";
import {Init, Contract, Value} from "@zilliqa-js/contract";
import {scillaContracts, ContractInfo} from "../ScillaContractProcessor";
import {TransitionParam, isNumeric} from "./ScillaParser";

// chain setup on ceres locally run isolated server, see https://dev.zilliqa.com/docs/dev/dev-tools-ceres/. Keys and wallet setup
const s = () => {
  let setup = {
    zilliqa: new Zilliqa("http://localhost:5555"),
    VERSION: bytes.pack(1, 1),
    addresses: [],
    pub_keys: [],
    priv_keys: [
      // b028055ea3bc78d759d10663da40d171dec992aa
      "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004",
      // f6dad9e193fa2959a849b81caf9cb6ecde466771":
      "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45"
    ]
  };
  setup["addresses"] = [];
  setup["pub_keys"] = [];
  setup.priv_keys.forEach((item) => {
    setup.zilliqa.wallet.addByPrivateKey(item); // add key to wallet
    setup.addresses.push(getAddressFromPrivateKey(item)); // compute and store address
    setup.pub_keys.push(getPubKeyFromPrivateKey(item)); // compute and store public key
  });
  return setup;
};
const setup = s();
exports.setup = setup;

// will use same tx settings for all tx's
const tx_settings = {
  gas_price: units.toQa("2000", units.Units.Li),
  gas_limit: Long.fromNumber(50000),
  attempts: 10,
  timeout: 1000
};

function read(f: string) {
  let t = fs.readFileSync(f, "utf8");
  return t;
}

export type ContractFunction<T = any> = (...args: Array<any>) => Promise<T>;

export class ScillaContract extends Contract {
  // Transitions
  [key: string]: ContractFunction | any;
}

export async function deploy(contractName: string, init?: Init) {
  let contractInfo: ContractInfo = scillaContracts[contractName];
  if (contractInfo === undefined) {
    throw new Error(`Scilla contract ${contractName} doesn't exist.`);
  }

  let sc: ScillaContract;
  if (init) {
    sc = await deploy_from_file(contractInfo.path, init);
  } else {
    const init = [{vname: "_scilla_version", type: "Uint32", value: "0"}];
    sc = await deploy_from_file(contractInfo.path, init);
  }

  contractInfo.transitions.forEach((transition) => {
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

    contractInfo.fields.forEach((field) => {
      sc[field.name] = async () => {
        const state = await sc.getState();
        if (isNumeric(field.type)) return Number(state[field.name]);
        return state[field.name];
      };
    });
  });

  return sc;
}

// deploy a smart contract whose code is in a file with given init arguments
async function deploy_from_file(path: string, init: Init): Promise<ScillaContract> {
  const code = read(path);
  const contract = setup.zilliqa.contracts.new(code, init);
  let [_, sc] = await contract.deploy(
    {version: setup.VERSION, gasPrice: tx_settings.gas_price, gasLimit: tx_settings.gas_limit},
    tx_settings.attempts,
    tx_settings.timeout,
    false
  );

  return sc;
}

// call a smart contract's transition with given args and an amount to send from a given public key
export async function sc_call(
  sc: Contract,
  transition: string,
  args: Value[] = [],
  amt = new BN(0),
  caller_pub_key = setup.pub_keys[0]
) {
  return sc.call(
    transition,
    args,
    {
      version: setup.VERSION,
      amount: amt,
      gasPrice: tx_settings.gas_price,
      gasLimit: tx_settings.gas_limit,
      pubKey: caller_pub_key
    },
    tx_settings.attempts,
    tx_settings.timeout,
    true
  );
}
