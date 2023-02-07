import {extendEnvironment, task} from "hardhat/config";
import "@nomicfoundation/hardhat-toolbox";
import "@nomiclabs/hardhat-web3";
import clc from "cli-color";
import {execSync} from "child_process";
import { glob } from "glob";

import yargs from "yargs/yargs";

const argv = yargs()
  .env()
  .options({
    debug: {
      type: "boolean",
      default: false
    },
    mochaWorkers: {
      type: "number",
      default: 4
    },
    mochaTimeout: {
      type: "number",
      default: 300000
    }
  })
  .parseSync();

/** @type import('hardhat/config').HardhatUserConfig */
const config: any = {
  solidity: "0.8.9",
  //defaultNetwork: "ganache",
  defaultNetwork: "isolated_server",
  networks: {
    // Corresponds to: provide kit tragic grid entry buffalo cherry balcony age exhibit pitch artwork
    ganache: {
      url: "http://localhost:7545",
      websocketUrl: "ws://localhost:7545",
      chainId: 1337,
      web3ClientVersion: "Ganache/v7.4.1/EthereumJS TestRPC/v7.4.1/ethereum-js",
      protocolVersion: 0x3f,
      accounts: [
        "17f8b324a522c4ae49e2a71ddee56fd887630d38af326db56eda46c82c45e709",
        "789a106e6217d2a8faa830422a3e0ea3004a6bbdb1c218dafad8e1b6f5a88308",
        "ac93195387641eed9d1d5523bebf5ee90faee84b8b3fb6e0816ab7d4858a31ca",
        "4d93cd9774b89831ebc81f7e94c20f499cb36a35b0a659b783c08e26be1f59d8"
      ],
      zilliqaNetwork: false,
      miningState: true
  },
  devnet: {
      url: "https://evmdev-l2api.dev.z7a.xyz",
      websocketUrl: "wss://evmdev-l2api.dev.z7a.xyz",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45"
      ],
      chainId: 33101,
      zilliqaNetwork: true,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      miningState: false
    },
    nathan: {
      url: "https://nathan-mod-l2api.dev.z7a.xyz",
      websocketUrl: "wss://nathan-mod-l2api.dev.z7a.xyz",
      accounts: [
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "e53d1c3edaffc7a7bab5418eb836cf75819a82872b4a1a0f1c7fcf5c3e020b89",
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411"
      ],
      chainId: 32769,
      zilliqaNetwork: true,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      miningState: false
    },
    public_testnet: {
      url: "https://evm-api-dev.zilliqa.com",
      websocketUrl: "https://evm-api-dev.zilliqa.com",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45"
      ],
      chainId: 33101,
      zilliqaNetwork: true,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      miningState: false
    },
    isolated_server: {
      url: "http://localhost:5555/",
      websocketUrl: "ws://localhost:5555/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14"
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    }
  },
  mocha: {
    timeout: argv.mochaTimeout,
    jobs: argv.mochaWorkers
  }
};

// Extend hardhat runtime environment to have some utility functions and variables.
import "./AddConfigHelpersToHre";
extendEnvironment((hre) => {
  hre.debug = argv.debug;
  hre.parallel = process.env.MOCHA_WORKER_ID !== undefined;
});

task("test")
  .addFlag("logJsonrpc", "Log JSON RPC ")
  .addFlag("logTxnid", "Log JSON RPC ")
  .setAction((taskArgs, hre, runSuper) => {
    if (taskArgs.logJsonrpc || taskArgs.logTxnid) {
      hre.ethers.provider.on("debug", (info) => {
        if (taskArgs.logJsonrpc) {
          if (info.request) {
            console.log("Request:", info.request);
          }
          if (info.response) {
            console.log("Response:", info.response);
          }
        }

        if (taskArgs.logTxnid) {
          if (info.request.method == "eth_sendTransaction" || info.request.method == "eth_sendRawTransaction") {
            console.log(clc.whiteBright.bold(`    📜 Txn ID: ${info.response}`));
          }
        }
      });
    }
    return runSuper();
  });

task("scilla-check", "Parsing scilla contracts and performing a number of static checks including typechecking.")
  .addParam("libdir", "Path to Scilla stdlib")
  .addOptionalVariadicPositionalParam("contracts", "An optional list of files to check", [])
  .setAction(async (taskArgs, hre, runSuper) => {
    let files: string[] = [];
    if (taskArgs.contracts.length === 0) {
      files = glob.sync("contracts/**/*.scilla");
    } else {
      files = taskArgs.contracts;
    }
    files.forEach((file) => {
      try {
        console.log(clc.greenBright.bold(`🔍Checking ${file}...`));
        const value = execSync(`scilla-checker -gaslimit 10000 -libdir ${taskArgs.libdir} ${file}`);
        console.log(value.toString());
      } catch (error) {
        console.error("Failed to run scilla-checker");
      }
    })
  });

export default config;
