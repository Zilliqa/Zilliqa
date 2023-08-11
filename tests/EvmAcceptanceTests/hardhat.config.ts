import "@nomicfoundation/hardhat-toolbox";
import "@nomiclabs/hardhat-web3";
import {HardhatUserConfig} from "hardhat/types";
import clc from "cli-color";
import "dotenv/config";
import {ENV_VARS} from "./helpers/EnvVarParser";
import semver from "semver";
import "./tasks/ZilBalance";
import "./tasks/Transfer";
import "./tasks/InitSigners";
import fs from "fs";

if (ENV_VARS.scilla) {
  require("hardhat-scilla-plugin");
  const chai = require("chai");
  const {scillaChaiEventMatcher} = require("hardhat-scilla-plugin");
  chai.use(scillaChaiEventMatcher);
}

declare module "hardhat/types/config" {
  interface HardhatNetworkUserConfig {
    websocketUrl?: string;
    web3ClientVersion?: string;
    protocolVersion: number;
    zilliqaNetwork: boolean;
    miningState: boolean;
  }
}

const loadFromSignersFile = (): string[] => {
  return fs
    .readFileSync(".signers")
    .toString()
    .split("\n")
    .filter((input) => input.length > 0);
};

const config: HardhatUserConfig = {
  solidity: "0.8.9",
  defaultNetwork: "isolated_server",
  networks: {
    localdev2: {
      url: "http://localdev-l2api.localdomain",
      websocketUrl: "ws://localdev-l2api.localdomain",
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
    },
    isolated_server: {
      url: "http://localhost:5555/",
      websocketUrl: "ws://localhost:5555/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        ...loadFromSignersFile()
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
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
    testnet: {
      url: "https://testnet-receipts-l2api.dev.z7a.xyz",
      websocketUrl: "wss://testnet-receipts-l2api.dev.z7a.xyz",
      accounts: [
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "e53d1c3edaffc7a7bab5418eb836cf75819a82872b4a1a0f1c7fcf5c3e020b89",
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14"
      ],
      chainId: 32769,
      zilliqaNetwork: true,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      miningState: false
    },
    local_network: {
      url: "http://localhost:8080",
      websocketUrl: "ws://localhost:8080",
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
    },
    localdev: {
      url: "http://localhost:5301",
      websocketUrl: "ws://localhost:5301",
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
    timeout: ENV_VARS.mochaTimeout,
    jobs: ENV_VARS.mochaWorkers
  }
};

// Extend hardhat runtime environment to have some utility functions and variables.
import "./AddConfigHelpersToHre";
import {extendEnvironment, task} from "hardhat/config";
import SignerPool from "./helpers/parallel-tests/SignerPool";
extendEnvironment(async (hre) => {
  hre.debug = ENV_VARS.debug;
  hre.parallel = process.env.MOCHA_WORKER_ID !== undefined;
  hre.scillaTesting = ENV_VARS.scilla;
  hre.signer_pool = new SignerPool();
  hre.signer_pool.initSigners(...(await hre.ethers.getSigners()));
});

task("test")
  .addFlag("logJsonrpc", "Log JSON RPC ")
  .addFlag("logTxnid", "Log JSON RPC ")
  .setAction(async (taskArgs, hre, runSuper): Promise<any> => {
    let signers = await hre.ethers.getSigners();
    let balances = await Promise.all(signers.map((signer) => signer.getBalance()));

    const node_version = process.version;
    if (semver.gt(node_version, "17.0.0")) {
      throw new Error(
        "⛔️" +
          clc.redBright.bold("Zilliqa-is incompatible with your current node version.") +
          "It should be >13.0.0 & <17.0.0."
      );
    }

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
    await runSuper();
    let newBalances = await Promise.all(signers.map((signer) => signer.getBalance()));
    let sum = balances
      .map((value, index) => value.sub(newBalances[index]))
      .reduce((prev, current) => prev.add(current));
    console.log(`  💰 ~${clc.blackBright.bold(hre.ethers.utils.formatEther(sum))} ZILs used`);
  });

export default config;
