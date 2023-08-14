import clc from "cli-color";
import { task } from "hardhat/config";
import semver from "semver";

task("test")
  .addFlag("logJsonrpc", "Log JSON RPC ")
  .addFlag("logTxnid", "Log JSON RPC ")
  .setAction(async (taskArgs, hre, runSuper): Promise<any> => {
    let signers = await hre.ethers.getSigners();
    hre.signer_pool.initSigners(...signers);
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

    if (taskArgs.parallel) {
      hre.parallel = true;
      await hre.run("parallel-test")
    }
    else 
      await runSuper();
    let newBalances = await Promise.all(signers.map((signer) => signer.getBalance()));
    let sum = balances
      .map((value, index) => value.sub(newBalances[index]))
      .reduce((prev, current) => prev.add(current));
    console.log(`  💰 ~${clc.blackBright.bold(hre.ethers.utils.formatEther(sum))} ZILs used`);
  });