import "@nomicfoundation/hardhat-toolbox";
import "@nomiclabs/hardhat-web3";
import clc from "cli-color";
import "dotenv/config";
import "hardhat-ethernal";
import {ENV_VARS} from "./helpers/EnvVarParser";

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

const config: HardhatUserConfig = {
  solidity: "0.8.9",

  ethernal: {
    disabled: ENV_VARS.ethernalPassword === undefined,
    email: ENV_VARS.ethernalEmail,
    password: ENV_VARS.ethernalPassword,
    workspace: ENV_VARS.ethernalWorkspace,
    disableSync: false, // If set to true, plugin will not sync blocks & txs
    disableTrace: false, // If set to true, plugin won't trace transaction
    uploadAst: true // If set to true, plugin will upload AST, and you'll be able to use the storage feature (longer sync time though)
  },
  defaultNetwork: "isolated_server",
  networks: {
    isolated_server: {
      url: "http://localhost:5555/",
      websocketUrl: "ws://localhost:5555/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        "385781136f89fb1ab3b5f2088e92292656677b7785e4785fc0cdfc85014fc122",
        "e7afd711bfe14f66e29d9b94cd28bfd7d536798ee5b5f0b6f42966cc607eef56",
        "2f341951053287cbba3a6953b184488d1870402cb3afd58e4e97ce4f1523ede3",
        "f5decff3e2fd8e9cc77679339e07256e32994d4a12e6cdd1c7761323095e360c",
        "9704de72300dac036050b96d9d41f5a9a20c967080c1a99c7db84dd1fb76b0ff",
        "30a62ffc55615538f87974ee19fccb08b0aee0de8267aa4107674cb3b3d680ac",
        "742444a015efd28191ee15239c7b8de373ed5c0cd3cd6b7e84b8abde1eb63e17",
        "b2f827428f70c189e67cbc9d12fa3640d5a72d47a1fa0979c2de1bea28957ac2",
        "d6caeec23fa216502d36338020f5db2da8e23f750fb47b2939cbdd63d5d365e3",
        "dfaece45664562554585432f9ce3b8185d5e649fca3217c9ab70c4b2e751c89d",
        "c65e2373078f3d28e9f43b281da24528f9ab16c6a6da7a24e23c43e9af3c60c6",
        "f2f64bd90b397864fbfcb5abbdef9f03fb4c0f366bc182a6316473b814c60dc4",
        "36fa20bd6efb719db2db4fe6fa032f4e27eaff2d93ab9c126c7c8052c3e9a9f8",
        "df39f323a84fc627e7b9709ebd092c3bbe83362c6099a5c0a8833b786d38ea7b",
        "2f12af0ade7abd2ca43491b172e6ce3ed3d4d842bfcad41aad175b272542758b",
        "951e9d978da755c7378e6592bab23a732c8bf6f69fd71bea876bf8bf222d3c86",
        "119b76812503662697f160470652eec7ac560b7096388d3ee6806f7903cd6225",
        "ddebebe59cac23d33b1845391b9121b0ce8208c3e1fe63832f125e7875f31265",
        "97441cd7afcb24cf024b2f686207a5136e6e126360ce1b2effa01bea9b1a7930",
        "fcb6e6ba825a577a164cb976a1461905978e42d6790bf46c664178f5523113a5"
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    },
    ganache: {
      url: "http://localhost:7545",
      websocketUrl: "ws://localhost:7545",
      chainId: 1337,
      web3ClientVersion: "Ganache/v7.4.1/EthereumJS TestRPC/v7.4.1/ethereum-js",
      protocolVersion: 0x3f,
      accounts: [
        // memonic: guard same cactus near figure photo remove letter target alien initial remove
        "67545ce31f5ca86719cf3743730435768515ebf014f84811463edcf7dcfaf91e",
        "9be4f8840833f64d4881027f4a53961d75bc649ac4801b33f746487ca8873f14",
        "32a75b674cc41405c914de1fe7b031b832dfd9203e1a287d09122bab689519e3",
        "dd8ce58f8cecd59fde7000fff9944908e89364b2ef36921c35725957617ddd32"
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
      url: "https://devnetnh-l2api.dev.z7a.xyz",
      websocketUrl: "wss://devnetnh-l2api.dev.z7a.xyz",
      accounts: [
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "e53d1c3edaffc7a7bab5418eb836cf75819a82872b4a1a0f1c7fcf5c3e020b89",
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "e53d1c3edaffc7a7bab5418eb836cf75819a82872b4a1a0f1c7fcf5c3e020b89"
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
    }
  },
  mocha: {
    timeout: ENV_VARS.mochaTimeout,
    jobs: ENV_VARS.mochaWorkers
  }
};

// Extend hardhat runtime environment to have some utility functions and variables.
import "./AddConfigHelpersToHre";
extendEnvironment((hre) => {
  hre.debug = ENV_VARS.debug;
  hre.parallel = process.env.MOCHA_WORKER_ID !== undefined;
  hre.scillaTesting = ENV_VARS.scilla;
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

export default config;
