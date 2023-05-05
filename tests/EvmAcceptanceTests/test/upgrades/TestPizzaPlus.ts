import hre,{ ethers, upgrades } from "hardhat"
import { getImplementationAddress } from "@openzeppelin/upgrades-core";

const SLICES=8;

describe("Test PizzaPlus UUPS contract", function () {
  let pizzaContract : Contract;

  before(async function () {
    const PizzaPlus = await ethers.getContractFactory("PizzaPlus");
    pizzaContract = await upgrades.deployProxy(PizzaPlus, [ SLICES ], {
      initializer: "initialize",
    });
    await pizzaContract.deployed();
  });

  it("Should be possible to eat a slice", async function() {
    await pizzaContract.eatSlice();
  });

  it("Should be possible to call getters", async function() {
    let result = await pizzaContract.getAddress();
    let beforeData = await result.wait();
    let resultPure = await pizzaContract.getAddressPure();
    console.log(`result = ${JSON.stringify(result)} R ${JSON.stringify(beforeData)} pure = ${JSON.stringify(resultPure)}`)
    await pizzaContract.setAddress();
    let afterResult = await pizzaContract.getAddress();
    let afterData = await afterResult.wait();
    let afterPure = await pizzaContract.getAddressPure();
    console.log(`After = ${JSON.stringify(afterResult)} D ${JSON.stringify(afterData)} AfterPure = ${JSON.stringify(afterPure)}`)
    const implAddress = await upgrades.erc1967.getImplementationAddress(pizzaContract.address);
    console.log(`Impl ${implAddress} proxy ${pizzaContract.address}`);
    let implContract = await hre.ethers.getContractAt("PizzaPlus", implAddress);
    let implPure = await implContract.getAddressPure();
    let implResult = await implContract.getAddress();
    let implData = await implResult.wait();
    console.log(`FromImpl = ${JSON.stringify(implData)} ImpPure = ${JSON.stringify(implPure)}`)
  });
});

