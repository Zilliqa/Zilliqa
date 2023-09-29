import chai, {expect} from "chai";
import {parallelizer} from "../../helpers";
import deepEqualInAnyOrder from "deep-equal-in-any-order";

chai.use(deepEqualInAnyOrder);

const getZilBalance = async (address: string) => {
  const zilliqa = parallelizer.zilliqaSetup.zilliqa;
  return zilliqa.blockchain.getBalance(address);
};

describe("Calling zilliqa GetBalance method", function () {
  it("should return error if account is not created yet", async function () {
    const account = parallelizer.zilliqaSetup.zilliqa.wallet.create();
    const balanceResult = await getZilBalance(account);
    expect(balanceResult.error).to.deep.equalInAnyOrder({
      code: -5,
      data: null,
      message: "Account is not created"
    });
  });

  it("should return the latest balance from the specified account", async function () {
    const account = parallelizer.zilliqaAccountAddress;
    const balanceResult = await getZilBalance(account);
    expect(+balanceResult.result.balance).to.be.gt(0);
  });
});
