import {initZilliqa, ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import {parallelizer} from "../../helpers";
import {getAddressFromPrivateKey} from "@zilliqa-js/crypto";
import {BN, Zilliqa, bytes, toChecksumAddress} from "@zilliqa-js/zilliqa";
import {Account} from "@zilliqa-js/account";
import clc from "cli-color";
import {ethers} from "ethers";
import Long from "long";
const { task } = require('hardhat/config');


const MSG_VERSION = 1; // current msgVersion
//const VERSION = bytes.pack(hre.getZilliqaChainId(), msgVersion);
const DEPLOYER_PRIVATE_KEY = "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357005";
const WELL_KNOWN_CONTRACT_ADDRESS = "0xb73DA094d60aa93AC4fA8Ae41Df5d8c13925b0BD".toLowerCase(); 

// NB: This test uses a hardwired key. It does this because the address of the contract is hardwired into
// the constants.xml file, and thus the nonce and identity of the deployer have to be known statically.
interface ContractContext {
  contract: ScilaContract;
  deployerAddress: String;
  zilliqa: Zilliqa;
  version: String;
  hre: HardhatRuntimeEnvironment;
  deployerAccount: Account
};

async function ensureContractDeployment(hre): ContractContext {
    // This is just one of the isolated server pubkeys incremented by 1.
    const funderPrivateKey = process.env.PRIMARY_ACCOUNT;
    if (funderPrivateKey == undefined) {
      throw Error(clc.bold.red("Please set PRIMARY_ACCOUNT environment variable before running this script."));
    }
  const zilliqa = new Zilliqa(hre.getNetworkUrl())
  const version = bytes.pack(hre.getZilliqaChainId(), MSG_VERSION);
  initZilliqa(hre.getNetworkUrl(), hre.getZilliqaChainId(), [ funderPrivateKey ], 30);
  const funderAddress = getAddressFromPrivateKey(funderPrivateKey);
  const deployerAddress = getAddressFromPrivateKey(DEPLOYER_PRIVATE_KEY);
  console.log(`Funding from: ${funderAddress}`);
  let deployerAccount = hre.zilliqa.pushPrivateKey(DEPLOYER_PRIVATE_KEY)[0];

  // Let's check for the (known) contract address.
  const contractBalanceResult = await zilliqa.blockchain.getBalance(WELL_KNOWN_CONTRACT_ADDRESS);
  if (!contractBalanceResult.error) {
    console.log(clc.green("Contract already deployed. Yay!"))
    const contract = await hre.interactWithScillaContract(WELL_KNOWN_CONTRACT_ADDRESS);
    return { contract, deployerAddress, zilliqa, version, hre, deployerAccount };
  }

  console.log(`Contract not present; deploying .. `);
  let deployerNeedsFunding = true;
  {
    const deployerBalance  = await zilliqa.blockchain.getBalance(deployerAddress);
    if (!deployerBalance.error) {
      // console.log(`Deployer nonce is ${deployerBalance.result.nonce}`);
      if (deployerBalance.result.nonce >= 1) {
        console.log(`Deployer nonce is ${clc.bold.red(deployerBalance.result.nonce)} - cannot deploy known contract. Try again with a new chain`);
        throw Error("Contract already deployed");
      }
      let bal = new BN(deployerBalance.result.balance);
      if (bal.gt(new BN("5_000_000_000"))) {
        console.log(`Deployer does not need funding`);
        deployerNeedsFunding = false;
      }
    }
  }

  const balanceResult = await zilliqa.blockchain.getBalance(funderAddress);
  if (balanceResult.error) {
    console.log(clc.bold.red(balanceResult.error.message));
    throw Error(`Coudln't get balance from funder address`);
  }
  const balance = new BN(balanceResult.result.balance);
  console.log(`Source has: ${clc.bold.green(balance)} and nonce ${clc.bold.green(balanceResult.result.nonce)}`);
  zilliqa.wallet.addByPrivateKey(funderPrivateKey);

  console.log(`Deployer is ${deployerAddress} ..`);

  zilliqa.wallet.setDefault(funderAddress);
  if (deployerNeedsFunding) {
    console.log(`Funding .. `);
    const tx = await zilliqa.blockchain.createTransactionWithoutConfirm(
    zilliqa.transactions.new(
      { version,
        toAddr: deployerAddress,
        amount: new BN("1_000_000_000_00"),
        gasPrice: new BN(2000000000), // Minimum gasPrice veries. Check the `GetMinimumGasPrice` on the blockchain
        gasLimit: Long.fromNumber(2100)
      },
      false
    ));
    if (tx.id) {
      const confirmedTxn = await tx.confirm(tx.id);
      const receipt = confirmedTxn.getReceipt();
      if (receipt && receipt.success) {
        console.log(`${deployerAddress}` + clc.bold.green("funded"));
      }
    } else {
      console.log(`Deployer could not be funded! ${JSON.stringify(tx)}`);
      throw Error(`Could not fund deployer`);
    }
  }
  // OK. Now deploy the contract as the deployer account, with nonce 1.
  hre.setActiveAccount(deployerAccount);
  console.log(`Deploying management contract from ${deployerAddress} with account ${deployerAccount.address}`);
  let contract = await hre.deployScillaContract("RewardsParams", deployerAddress);
  console.log(`Result ${JSON.stringify(contract)}`);
  console.log(`Contract deployed at ${contract.address}`);
  return { contract, deployerAddress, zilliqa, version, hre, deployerAccount };
};

async function checkContractDeployment() {
  const contractBalance  = await zilliqa.blockchain.getBalance(WELL_KNOWN_CONTRACT_ADDRESS);
  console.log(`Bal ${JSON.stringify(contractBalance)}`);
  if (contractBalance.error !== undefined) {
    throw Error(`Control contract not deployed! ${JSON.stringify(contractBalance)}`);
  }
}


async function adjustRewards(context: ContractContext) {
  let promises = [];

  if (process.env.COINBASE_REWARD_PER_DS) {
    let newReward = process.env.COINBASE_REWARD_PER_DS;
    console.log(clc.yellow(`Changing COINBASE_REWARD_PER_DS to ${newReward}`));
    promises.push(context.contract.ChangeCoinbaseReward(newReward));
  }

  console.log(clc.green("Waiting for changes to propagate .. "));
  let results = [];
  for (let p of promises) {
    let result = await p;
    console.log(clc.green(`${JSON.stringify(result)}`));
  }
}

async function main(hre: HardhatRuntimeEnvironment) {
  let context = await ensureContractDeployment(hre);
  await adjustRewards(context);
}

task('set-rewards', 'Sets the chain reward parameters')
  .addOptionalParam("coinbaserewardperds", "Set the coinbase reward to .. ")
  .addOptionalParam("baserewardscaledpercent", "Set the base reward to (DANGER! Check the percent_precision first!)")
  .addOptionalParam("lookuprewardscaledpercent", "Set the lookup (SSN) reward (DANGER! Check percent_precision first!)")
  .addOptionalParam("rewardeachmillis", "Set the fast miner reward ratio, in millis")
  .addOptionalParam("baserewardmillis", "Set the base miner reward ratio, in millis")
  .setAction(async (taskArgs, hre) => {
    let context = await ensureContractDeployment(hre);
    let percentPrecision = await context.contract.getSubState("percent_precision");
    console.log(clc.green(`Percent precision is ${percentPrecision.percent_precision}`));
    context.zilliqa.wallet.setDefault(context.deployerAddress);
    let connected = context.contract.connect(context.deployerAccount);
    if (taskArgs.coinbaserewardperds !== undefined) {
      let newReward = taskArgs.coinbaserewardperds;
      console.log(clc.yellow(`Changing COINBASE_REWARD_PER_DS to ${newReward}`));
      let result = await connected.ChangeCoinbaseReward(newReward);
      console.log(clc.green(`${JSON.stringify(result)}`));
    }
    if (taskArgs.baserewardscaledpercent !== undefined) {
      let newReward = taskArgs.baserewardscaledpercent;
      console.log(clc.yellow(`Changing base reward scaled percent to ${newReward}`));
      let result = await connected.ChangeBaseReward(newReward);
      console.log(clc.green(`${JSON.stringify(result)}`));
    }
    if (taskArgs.lookuprewardscaledpercent !== undefined) {
      let newReward = taskArgs.lookuprewardscaledpercent;
      console.log(clc.yellow(`Changing lookup reward scaled percent to ${newReward}`));
      let result = await connected.ChangeLookupReward(newReward);
      console.log(clc.green(`${JSON.stringify(result)}`));
    }
    if (taskArgs.rewardeachmillis !== undefined) {
      let newReward = taskArgs.rewardeachmillis;
      console.log(clc.yellow(`Changing fast miner reward ratio in millis to ${newReward}`));
      let result = await connected.ChangeRewardEachMulInMillis(newReward);
      console.log(clc.green(`${JSON.stringify(result)}`));
    }
    if (taskArgs.baserewardmillis !== undefined) {
      let newReward = taskArgs.baserewardmillis;
      console.log(clc.yellow(`Changing base miner reward ratio in millis to ${newReward}`));
      let result = await connected.ChangeBaseRewardMulInMillis(newReward);
      console.log(clc.green(`${JSON.stringify(result)}`));
    }
    
  });

