import hre,{ ethers, upgrades } from "hardhat"
import {expect} from "chai";

const SLICES=8;

describe("Test Pizza UUPS contract", function () {
  let pizzaContract : Contract;

  before(async function () {
    const Pizza = await ethers.getContractFactory("PizzaUUPS");
    pizzaContract = await upgrades.deployProxy(Pizza, [ SLICES ], {
      initializer: "initialize",
    });
    await pizzaContract.deployed();
  });

  it("Should be possible to eat a slice", async function() {
    const txn = await pizzaContract.eatSlice();
    const receipt = await txn.wait();
    // I would like to check for success, but that doesn't seem to work :-(
    expect(receipt.status).to.equal(1);
    const remain = await pizzaContract.getSlices();
    expect(remain).to.equal(7);
  });
});

