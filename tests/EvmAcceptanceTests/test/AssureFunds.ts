import {expect} from "chai";
import hre from "hardhat";
import LogDebug from "../helpers/DebugHelper";

describe("Assure funds in contract accounts are adequate", function () {

  it("Should exist and have enough funds", async function () {

    const {provider} = hre.network;

    const accounts: string[] = await provider.send("eth_accounts");
    let balances = await Promise.all(
      accounts.map((account: string) => provider.send("eth_getBalance", [account, "latest"]))
    );

    // Print balances of eth accounts before
    accounts.forEach((element, index) => {
      LogDebug("Account: ", element);
      LogDebug("Balance: ", balances[index]);
      expect(BigInt(balances[index])).to.be.greaterThan(BigInt(0))
    });

  });
});
