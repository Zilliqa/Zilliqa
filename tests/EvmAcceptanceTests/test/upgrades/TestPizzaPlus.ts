import hre,{ ethers, upgrades } from "hardhat"
import { getImplementationAddress } from "@openzeppelin/upgrades-core";
import {expect} from "chai";

const DEBUG=false;

const SLICES=8;
const ZERO_ADDRESS="0x0000000000000000000000000000000000000000"

describe("Test PizzaPlus UUPS contract", function () {
  let pizzaContract : Contract;

  before(async function () {
    const PizzaPlus = await ethers.getContractFactory("PizzaPlusUUPS");
    pizzaContract = await upgrades.deployProxy(PizzaPlus, [ SLICES ], {
      initializer: "initialize",
    });
    await pizzaContract.deployed();
  });

  it("Should be possible to eat a slice", async function() {
    await expect(pizzaContract.eatSlice()).not.to.be.reverted;
  });

  it("Should be possible to call getters", async function() {
    let signer = await pizzaContract.signer;
    if (DEBUG) {
      console.log(`Signer is ${signer.address}`)
    }
    let result = await pizzaContract.getAddress();
    let beforeData = await result.wait();
    let resultPure = await pizzaContract.getAddressPure();
    // The initial address should be 0.
    expect(resultPure).to.equal(ZERO_ADDRESS)
    await expect(pizzaContract.setAddress()).not.to.be.reverted;
    let afterResult = await pizzaContract.getAddress();
    let afterData = await afterResult.wait();
    // Should have worked.
    let afterPure = await pizzaContract.getAddressPure();
    // The getAddress() should work and afterPure should be equal to signer.address.
    expect(afterData.status).to.equal(1);
    expect(afterPure).to.equal(signer.address);

    const implAddress = await upgrades.erc1967.getImplementationAddress(pizzaContract.address);
    let implContract = await hre.ethers.getContractAt("PizzaPlusUUPS", implAddress);
    let implPure = await implContract.getAddressPure();
    let implResult = await implContract.getAddress();
    let implReceipt = await implResult.wait();
    // The transaction should emit no events, because addresses in the proxy is empty
    expect(implResult).to.emit(implContract, "AddressIsNot");
    // The pure access should be 0, because the state is held in the proxy
    expect(implPure).to.equal(ZERO_ADDRESS);
  });
});

