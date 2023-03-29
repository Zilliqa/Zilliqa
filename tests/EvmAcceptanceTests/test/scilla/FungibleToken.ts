import {ScillaContract} from "hardhat-scilla-plugin";
import {expect} from "chai";
import hre from "hardhat";
import {parallelizer} from "../../helpers";

describe("Scilla HelloWorld contract", function () {
  let contract: ScillaContract;
  let aliceAddress: string;
  let bobAddress: string;
  let ownerAddress: string;
  const getAllowance = async (sender: string, spender: string): Promise<number> => {
    return Number((await contract.allowances())[sender.toLowerCase()][spender.toLowerCase()])
  }

  const getBalance = async (address: string): Promise<number> => {
    return Number((await contract.balances())[address.toLowerCase()])
  }

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contract = await parallelizer.deployScillaContract("FungibleToken", parallelizer.zilliqaAccountAddress, "Saeed's Token", "SDT", 2, 1_000);
    aliceAddress = parallelizer.zilliqaSetup.zilliqa.wallet.create().toLocaleLowerCase()
    bobAddress = parallelizer.zilliqaSetup.zilliqa.wallet.create().toLocaleLowerCase()
    ownerAddress = parallelizer.zilliqaAccountAddress;
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Should have Saeed's Token as its name", async function () {
    expect(await contract.name()).to.be.eq("Saeed's Token")
  });

  it("Should have SDT as its symbol", async function () {
    expect(await contract.symbol()).to.be.eq("SDT")
  });

  it("Should have 1000 as contract owner's balance", async function () {
    const owner_balance = (await contract.balances())[parallelizer.zilliqaAccountAddress.toLowerCase()]
    expect(Number(owner_balance)).to.be.eq(1000)
  });

  it("Should be possible to increase allowance by contract owner", async function () {
    await contract.IncreaseAllowance(aliceAddress, 100);

    expect(await getAllowance(ownerAddress, aliceAddress)).to.be.eq(100)
  });

  it("Should be possible to decrease allowance by contract owner", async function () {
    await contract.DecreaseAllowance(aliceAddress, 10);

    expect(await getAllowance(ownerAddress, aliceAddress)).to.be.eq(90);
  });

  it("Should be possible to transfer", async function () {
    // let privkey = '07e0b1d1870a0ba1b60311323cb9c198d6f6193b2219381c189afab3f5ac41a9';
    // parallelizer.zilliqaSetup.zilliqa.wallet.addByPrivateKey(privkey);
    const address = "0xBFe2445408C51CD8Ee6727541195b02c891109ee";

    let x = await contract.Transfer(address, 10);

    expect(await getBalance(address)).to.be.eq(10)
  });
});
