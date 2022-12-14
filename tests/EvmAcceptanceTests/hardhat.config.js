const clc = require("cli-color");
//ceiling energy brass such rescue brief pound wrist tone chat high midnight

require("@nomicfoundation/hardhat-toolbox");
require("@nomiclabs/hardhat-web3");
require("hardhat-gas-reporter");
require("solidity-coverage");
require("@openzeppelin/hardhat-upgrades");

/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  solidity: {
    version: "0.8.17",
    settings: {
      optimizer: {
        enabled: true,
        runs: 200
      }
    }
  },
  //defaultNetwork: "ganache",
  defaultNetwork: "isolated_server",
  networks: {
    ganache: {
      url: "http://localhost:7545",
      websocketUrl: "ws://localhost:7545",
      chainId: 1337,
      web3ClientVersion: "Ganache/v7.4.1/EthereumJS TestRPC/v7.4.1/ethereum-js",
      protocolVersion: 0x3f,
      accounts: [
        "c75c9269aa768794d24bc8f88e01c58f463ac3412895367609b5c4f099e8d829",
        "5c219a0c5ff246f3705df936d15e5b0ab0ef1c34baaed1a54cf483ab836a65a6",
        "d0390e1e71d4623a1aa830bdbe2db7bd882bf3efdcf82ad31cfb338bb2777d2f",
        "1a664a5d8fd0808fc8cf1209b68f14e5b84cc6fb4908a40680d3d29bb6fc221e",
        "24378c6e5dfa2478334a4cf3d4cb15b8b88ff53cd8a06a2327c171bb49db0dcd"
      ],
      zilliqaNetwork: false,
      miningState: true
    },
    goerli: {
      url: "https://goerli.infura.io/v3/9aa3d95b3bc440fa88ea12eaa4456161",
      timeout: 60000000,
      gas: 30000000,
      gasPrice: 8000000000, // 8 gwei
      accounts: [
        "a8b68f4800bc7513fca14a752324e41b2fa0a7c06e80603aac9e5961e757d906",
        "a40d92be3d97aa04ed7dc08f526bac4c67a7b642f32017d4adc76e20b2fd44db",
        "5e86096dd8ea984ad71234e1fe0d92b3f976888fbe07f72a052b8a1477178508",
        "c33c6b1e80e9536b26a28e5b08550b99772a6e3d869ba5251d5cbbd9069e3f60",
        "cb3f0dbba52fbe32f5d9713da3686272b784461bafe5e78d58a83d80b4c551c2"
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
    isolated_server: {
      url: "http://localhost:5555/",
      websocketUrl: "ws://localhost:5555/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "410b0e0a86625a10c554f8248a77c7198917bd9135c15bb28922684826bb9f14",
        "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3"
      ],
      chainId: 0x8001,
      web3ClientVersion: "Zilliqa/v8.2",
      protocolVersion: 0x41,
      zilliqaNetwork: true,
      miningState: false
    }
  },
  mocha: {
    timeout: 300000
  }
};

task("test")
  .addFlag("debug", "Print debugging logs")
  .addFlag("logJsonrpc", "Log JSON RPC ")
  .addFlag("logTxnid", "Log JSON RPC ")
  .setAction(async (taskArgs, hre, runSuper) => {
    hre.debugMode = taskArgs.debug ?? false;
    hre.logDebug = hre.debugMode ? console.log.bind(console) : function () {};
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
