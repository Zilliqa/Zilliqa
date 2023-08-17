import hre from "hardhat";
import clc from "cli-color";
import {ethers} from "ethers";

async function main() {
  const {provider} = hre.network;

  const accounts: string[] = await provider.send("eth_accounts");
  const balances = await Promise.all(
    accounts.map((account: string) => provider.send("eth_getBalance", [account, "latest"]))
  );
  accounts.forEach((element, index) => {
    console.log(
      clc.bold("Account"),
      clc.green(element),
      clc.bold("Balance:"),
      clc.greenBright(ethers.BigNumber.from(balances[index])).toString()
    );
  });
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
