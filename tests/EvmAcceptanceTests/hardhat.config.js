require("@nomicfoundation/hardhat-toolbox");
require("@nomiclabs/hardhat-web3");

/** @type import('hardhat/config').HardhatUserConfig */
module.exports = {
  solidity: "0.8.9",
  defaultNetwork: "isolated_server",
  networks: {
    zilliqa: {
      url: "https://master-mod-api.dev.z7a.xyz/",
      accounts: ["d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba"],
      chainId: 33101
    },
    isolated_server: {
      url: "http://localhost:5555/",
      accounts: ["d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
                 "5c497e53404aca018264588643a3b6b48c6a03579da1538628acf827c4a264b3"
       ],
      chainId: 0x8001,
    },
    evmdev: {
      url: "https://evmdev-api.dev.z7a.xyz/",
      accounts: ["d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba"],
      chainId: 33101
    }
  }
};
