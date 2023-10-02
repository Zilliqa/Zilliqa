import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";
import {Account} from "@zilliqa-js/zilliqa";

describe("Scilla Fungible token contract", function () {
  let contract: ScillaContract;
  let signer: Account;
  let null_contract: ScillaContract;
  let aliceAddress: string;
  let bobAddress: string;
  let ownerAddress: string;
  const getAllowance = async (sender: string, spender: string): Promise<number> => {
    return Number((await contract.allowances())[sender.toLowerCase()][spender.toLowerCase()]);
  };

  const getBalance = async (address: string): Promise<number> => {
    return Number((await contract.balances())[address.toLowerCase()]);
  };

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    signer = hre.allocateZilSigner();

    contract = await hre.deployScillaContractWithSigner(
      "FungibleToken",
      signer,
      signer.address,
      "Saeed's Token",
      "SDT",
      2,
      1_000
    );
    null_contract = await parallelizer.deployScillaContract("HelloWorld", "0xBFe2445408C51CD8Ee6727541195b02c891109ee");
    aliceAddress = parallelizer.zilliqaSetup.zilliqa.wallet.create().toLocaleLowerCase();
    bobAddress = parallelizer.zilliqaSetup.zilliqa.wallet.create().toLocaleLowerCase();
    ownerAddress = signer.address;
  });

  after(function () {
    hre.releaseZilSigner(signer);
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should have Saeed's Token as its name", async function () {
    expect(await contract.name()).to.be.eq("Saeed's Token");
  });

  it("Should have SDT as its symbol", async function () {
    expect(await contract.symbol()).to.be.eq("SDT");
  });

  it("Should have 1000 as contract owner's balance", async function () {
    const owner_balance = (await contract.balances())[ownerAddress.toLowerCase()];
    expect(Number(owner_balance)).to.be.eq(1000);
  });

  it("Should be possible to increase allowance by contract owner", async function () {
    await contract.IncreaseAllowance(aliceAddress, 100);

    expect(await getAllowance(ownerAddress, aliceAddress)).to.be.eq(100);
  });

  it("Should be possible to decrease allowance by contract owner", async function () {
    await contract.DecreaseAllowance(aliceAddress, 10);

    expect(await getAllowance(ownerAddress, aliceAddress)).to.be.eq(90);
  });

  it("Should be possible to transfer", async function () {
    const address = "0xBFe2445408C51CD8Ee6727541195b02c891109ee";

    try {
      await contract.Transfer(address, 10);
    } catch (error) {
      console.log("Error: ", error);
    }

    expect(await getBalance(address)).to.be.eq(10);
  });

  it("Should not be possible to transfer to a contract which does not accept", async function () {
    const CALL_CONTRACT_FAILED = 7;
    try {
      let result = await contract.Transfer(null_contract.address, 10);
      expect(result.receipt.success).to.be.false;
      expect(result.receipt.errors["1"].length).to.equal(1);
      expect(result.receipt.errors["1"][0]).to.equal(CALL_CONTRACT_FAILED);
    } catch (error) {
      console.log("Error: ", error);
    }
  });
});
