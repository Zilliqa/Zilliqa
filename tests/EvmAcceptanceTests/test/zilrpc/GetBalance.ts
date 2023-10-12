import chai, {expect} from "chai";
import deepEqualInAnyOrder from "deep-equal-in-any-order";
import hre from "hardhat";

chai.use(deepEqualInAnyOrder);

const getZilBalance = async (address: string) => {
  const zilliqa = hre.zilliqaSetup.zilliqa;
  return zilliqa.blockchain.getBalance(address);
};

describe("Calling zilliqa GetBalance method #parallel", function () {
  it("should return error if account is not created yet @block-1", async function () {
    const account = hre.zilliqaSetup.zilliqa.wallet.create();
    const balanceResult = await getZilBalance(account);
    expect(balanceResult.error).to.deep.equalInAnyOrder({
      code: -5,
      data: null,
      message: "Account is not created"
    });
  });

  it("should return the latest balance from the specified account @block-1", async function () {
    const account = hre.allocateZilSigner();
    const balanceResult = await getZilBalance(account.address);
    expect(+balanceResult.result.balance).to.be.gt(0);
    hre.releaseZilSigner(account);
  });
});
