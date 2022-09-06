require("@nomicfoundation/hardhat-toolbox");
require("@nomiclabs/hardhat-web3");

/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  solidity: "0.8.9",
  defaultNetwork: "ganache",
  // defaultNetwork: "isolated_server",
  networks: {
    ganache: {
      url: "http://localhost:7545",
      chainId: 1337,
      web3_clientVersion: "Ganache/v7.4.1/EthereumJS TestRPC/v7.4.1/ethereum-js",
      protocolVersion: 0x3f,
      accounts: [
        "c95690aed4461afd835b17492ff889af72267a8bdf7d781e305576cd8f7eb182",
        "05751249685e856287c2b2b9346e70a70e1d750bc69a35cef740f409ad0264ad"
      ]
    },
    zilliqa: {
      url: "https://master-mod-api.dev.z7a.xyz/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba"
      ],
      chainId: 33101,
      web3_clientVersion: "Zilliqa/v1.0",
      protocolVersion: "0x41"
    },
    isolated_server: {
      url: "http://localhost:5555/",
      accounts: [
        "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "5c497e53404aca018264588643a3b6b48c6a03579da1538628acf827c4a264b3"
      ],
      chainId: 0x8001,
      web3_clientVersion: "Zilliqa/v8.1.2",
      protocolVersion: 0x41
    }
  }
};
