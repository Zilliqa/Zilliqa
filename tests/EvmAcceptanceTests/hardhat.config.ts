import "@nomicfoundation/hardhat-toolbox";
import "@nomiclabs/hardhat-web3";
import {HardhatUserConfig} from "hardhat/types";
import "dotenv/config";
import {ENV_VARS} from "./helpers/EnvVarParser";
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
    public_devnet: {
      url: "https://api.devnet.zilliqa.com",
      websocketUrl: "ws://api.devnet.zilliqa.com",
      accounts: [
        "4CC853DE4F9FE4A9155185C65B56B6A9B024F896B54528B9E9448B6CD9B8F329",
        ...loadFromSignersFile("public_devnet")
      ],
      chainId: 33385,
      zilliqaNetwork: true,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      miningState: false
    },
    localdev2: {
      url: "http://localdev-l2api.localdomain",
      websocketUrl: "ws://localdev-l2api.localdomain",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        ...loadFromSignersFile("localdev2")
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    },
    isolated_server: {
      url: "http://127.0.0.1:5555/",
      websocketUrl: "ws://127.0.0.1:5555/",
      accounts: [
        ...loadFromSignersFile("isolated_server")
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    },
    public_testnet: {
      url: "https://evm-api-dev.zilliqa.com",
      websocketUrl: "https://evm-api-dev.zilliqa.com",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        ...loadFromSignersFile("public_testnet")
      ],
      chainId: 33101,
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
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        ...loadFromSignersFile("local_network")
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
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        ...loadFromSignersFile("localdev")
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    }
  },
  mocha: {
    reporter: ENV_VARS.mochaReporter,
    timeout: ENV_VARS.mochaTimeout,
    jobs: ENV_VARS.mochaWorkers
  }
};

// Extend hardhat runtime environment to have some utility functions and variables.
import "./AddConfigHelpersToHre";
import {extendEnvironment} from "hardhat/config";
import SignerPool from "./helpers/parallel-tests/SignerPool";
extendEnvironment(async (hre) => {
  const private_keys: string[] = hre.network["config"]["accounts"] as string[];

  hre.debug = ENV_VARS.debug;
  hre.scillaTesting = ENV_VARS.scilla;
  hre.signer_pool = new SignerPool();
  hre.zilliqaSetup = initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), private_keys, 30);
});

import "./tasks/Balances";
import "./tasks/Setup";
import "./tasks/ParallelTest";
import "./tasks/Test";
import "./tasks/ZilBalance";
import "./tasks/Transfer";
import "./tasks/InitSigners";
import { initZilliqa } from "hardhat-scilla-plugin";
export default config;
