import {expect} from "chai";
import hre from "hardhat";
import {initZilliqa} from "hardhat-scilla-plugin";

const {BN, Long, bytes, units} = require("@zilliqa-js/util");
const {Zilliqa} = require("@zilliqa-js/zilliqa");
const {toBech32Address, getAddressFromPrivateKey} = require("@zilliqa-js/crypto");

async function main() {
  // Deploy contract

  const NETWORK = "ISO_SERVER";

  let privkey;
  let hostname: string;
  let chainid;
  if (NETWORK === "DEVNET") {
    privkey = "07e0b1d1870a0ba1b60311323cb9c198d6f6193b2219381c189afab3f5ac41a9";
    hostname = "https://dev-api.zilliqa.com";
    chainid = 333;
  } /*if (NETWORK === "ISO_SERVER")*/ else {
    privkey = "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004";
    hostname = "http://localhost:5555";
    chainid = 1;
  }

  const zilliqaSetup = initZilliqa(hostname, chainid, [privkey]);

  const address = getAddressFromPrivateKey(privkey);
  let contract = await hre.deployScilla("FungibleToken", address, "Saeed's Token", "SDT", 2, 1_000);

  zilliqaSetup.zilliqa.wallet.addByPrivateKey(privkey);

  console.log("Your account address is:");
  console.log(`${address}`);
  const balance = await zilliqaSetup.zilliqa.blockchain.getBalance(address);
  console.log(`Your balance is ${balance.result.balance}`);

  const userAddress = "0xBFe2445408C51CD8Ee6727541195b02c891109ee";
  await contract.Transfer(userAddress, 100);
  const balances = await contract.balances();
  console.log(await contract.balances());
  expect(Number(balances[userAddress.toLowerCase()])).to.be.eq(100);

  // const ftAddr = toBech32Address("509ae6e5d91cee3c6571dcd04aa08288a29d563a");
  // try {
  //     // const contract = zilliqa.contracts.at(ftAddr);
  //     const callTx = await contract.call(
  //         'Transfer',
  //         [
  //             {
  //                 vname: 'to',
  //                 type: 'ByStr20',
  //                 value: "0xBFe2445408C51CD8Ee6727541195b02c891109ee",
  //             },
  //             {
  //                 vname: 'amount',
  //                 type: 'Uint128',
  //                 value: "100",
  //             }
  //         ],
  //         {
  //             // amount, gasPrice and gasLimit must be explicitly provided
  //             version: VERSION,
  //             amount: new BN(0),
  //             gasPrice: myGasPrice,
  //             gasLimit: Long.fromNumber(10000),
  //         }
  //     );
  //     console.log("SUCCESS", JSON.stringify(callTx.receipt, null, 4));
  //     console.log("CONTRACT*******", contract)

  // } catch (err) {
  //     console.log("EEEERRRRORR", err);
  // }
}

main();
