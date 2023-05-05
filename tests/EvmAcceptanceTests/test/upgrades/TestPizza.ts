import hre,{ ethers, upgrades } from "hardhat"

const SLICES=8;

describe("Test Pizza UUPS contract", function () {
  let pizzaContract : Contract;

  before(async function () {
    const Pizza = await ethers.getContractFactory("Pizza");
    pizzaContract = await upgrades.deployProxy(Pizza, [ SLICES ], {
      initializer: "initialize",
    });
    await pizzaContract.deployed();
  });

  it("Should be possible to eat a slice", async function() {
    await pizzaContract.eatSlice();
  });
});

