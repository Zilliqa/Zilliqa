import {expect} from "chai";
import {Contract} from "ethers";
import {parallelizer} from "../helpers";

describe("Chained Contract Calls Functionality", function () {
  //const INITIAL_FUND = 10_000_000;
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;

  before(async function () {
    contractOne = await parallelizer.deployContract("ContractOne");
    contractTwo = await parallelizer.deployContract("ContractTwo");
    contractThree = await parallelizer.deployContract("ContractThree");
  });

  describe("Install and call chained contracts", function () {
    it("Should correctly call chained contracts", async function () {
      let addrOne = contractOne.address.toLowerCase();
      let addrTwo = contractTwo.address.toLowerCase();
      let addrThree = contractThree.address.toLowerCase();

      expect(contractOne.chainedCall([addrTwo, addrThree, addrOne], 0)).to.emit(contractOne, "FinalMessage");
    });
  });
});
