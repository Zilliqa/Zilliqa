import "@nomicfoundation/hardhat-toolbox";
import "@nomiclabs/hardhat-web3";
import {HardhatUserConfig} from "hardhat/types";
import "dotenv/config";
import {ENV_VARS} from "./helpers/EnvVarParser";
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

const loadFromSignersFile = (network_name: string): string[] => {
  try {
    return JSON.parse(fs.readFileSync(`.signers/${network_name}.json`, "utf8"));
  } catch (error) {
    return [];
  }
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
        ...loadFromSignersFile("isolated_server")
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
import {extendEnvironment} from "hardhat/config";
import SignerPool from "./helpers/parallel-tests/SignerPool";
extendEnvironment(async (hre) => {
  hre.debug = ENV_VARS.debug;
  hre.scillaTesting = ENV_VARS.scilla;
  hre.signer_pool = new SignerPool();
});

import "./tasks/Balances"; // To fix tsc error
import "./tasks/Setup"; // To fix tsc error
import "./tasks/ParallelTest";
import "./tasks/Test";
export default config;
