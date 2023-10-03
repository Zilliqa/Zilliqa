import clc from "cli-color";
import {task} from "hardhat/config";

task("test")
  .addFlag("logJsonrpc", "Log JSON RPC ")
  .addFlag("logTxnid", "Log JSON RPC ")
  .setAction(async (taskArgs, hre, runSuper): Promise<any> => {
    console.log(clc.yellow(`⚠️  Your ${clc.bold("Eth/Zil")} accounts supposed to have funds to run tests successfully.`));
    let signers = await hre.ethers.getSigners();
    const private_keys: string[] = hre.network["config"]["accounts"] as string[];
    hre.signer_pool.initSigners(signers, private_keys);
    let balances = await Promise.all(signers.map((signer) => signer.getBalance()));

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
      await hre.run("parallel-test", taskArgs);
    } else await runSuper();
    let newBalances = await Promise.all(signers.map((signer) => signer.getBalance()));
    let sum = balances
      .map((value, index) => value.sub(newBalances[index]))
      .reduce((prev, current) => prev.add(current));
    console.log(`💰 ~${clc.blackBright.bold(hre.ethers.utils.formatEther(sum))} ZILs used`);
  });
