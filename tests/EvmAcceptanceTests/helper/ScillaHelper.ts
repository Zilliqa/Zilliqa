import {Zilliqa} from "@zilliqa-js/zilliqa";
import fs from "fs";
import {BN, Long, units, bytes} from "@zilliqa-js/util";
import {getAddressFromPrivateKey, getPubKeyFromPrivateKey} from "@zilliqa-js/crypto";
import { Init } from "@zilliqa-js/contract";

// chain setup on ceres locally run isolated server, see https://dev.zilliqa.com/docs/dev/dev-tools-ceres/. Keys and wallet setup
const s = () => {
  let setup = {
    zilliqa: new Zilliqa("http://localhost:5555"),
    VERSION: bytes.pack(1, 1),
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
  attempts: Long.fromNumber(10),
  timeout: 1000
};

/* ---------------------------------------------------------------------------------------------------------------------------
utility functions
--------------------------------------------------------------------------------------------------------------------------- */
// read a file and return contents as a string
function read(f: string) {
  let t = fs.readFileSync(f, "utf8", (err, txt) => {
    if (err) throw err;
  });
  return t;
}

// deploy a smart contract whose code is in a file with given init arguments
export async function deploy_from_file(path: string, init: Init) {
  const code = read(path);
  const contract = setup.zilliqa.contracts.new(code, init);
  let [_, sc] = await contract.deploy(
    {version: setup.VERSION, gasPrice: tx_settings.gas_price, gasLimit: tx_settings.gas_limit},
    tx_settings.attempts,
    tx_settings.timeout,
    false
  );

  sc.Set = async function(v: number) {
    const args = [{vname: "v", type: "Uint128", value: v.toString()}];
    await sc_call(sc, "Set", args);
    const state = await sc.getState();
    return state;
  }
  return sc;
}

// deploy a scilla lib whose code is in a string
// async function deploy_lib_from_file(path) {
//   const lib_init = [
//     {vname: "_scilla_version", type: "Uint32", value: "0"},
//     {vname: "_library", type: "Bool", value: {constructor: "True", argtypes: [], arguments: []}}
//   ];
//   const code = read(path);
//   const user_lib = setup.zilliqa.contracts.new(code, lib_init);
//   return user_lib.deploy(
//     // Deployment
//     {version: setup.VERSION, gasPrice: tx_settings.gas_price, gasLimit: tx_settings.gas_limit},
//     tx_settings.attempts,
//     tx_settings.timeoute,
//     false
//   );
// }

// call a smart contract's transition with given args and an amount to send from a given public key
export async function sc_call(sc, transition, args = [], amt = new BN(0), caller_pub_key = setup.pub_keys[0]) {
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
