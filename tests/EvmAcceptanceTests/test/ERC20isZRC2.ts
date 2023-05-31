import {expect} from "chai";
import {Contract, Signer} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";

describe.skip("ERC20 Is ZRC2", function () {
  let zrc2_contract: ScillaContract;
  let erc20_contract: Contract;
  let contractOwner: Signer;
  let alice: Signer;
  let bob: Signer;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contractOwner = await parallelizer.takeSigner();

    zrc2_contract = await parallelizer.deployScillaContract(
      "FungibleToken",
      await contractOwner.getAddress(),
      "ERC20isZRC2 Token",
      "SDT",
      2,
      1_000
    );
    alice = await parallelizer.takeSigner();
    bob = await parallelizer.takeSigner();
    erc20_contract = await parallelizer.deployContractWithSigner(
      contractOwner,
      "ERC20isZRC2",
      zrc2_contract.address?.toLowerCase()
    );
  });

  it("Interop Should be deployed successfully", async function () {
    expect(zrc2_contract.address).to.be.properAddress;
    expect(erc20_contract.address).to.be.properAddress;
  });

  it("should return correct contract owner from ZRC2", async function () {
    expect(await zrc2_contract.contract_owner()).to.be.eq(await contractOwner.getAddress());
  });

  it("Should return zrc2/erc20 total supply via bridge contract", async function () {
    expect(await erc20_contract.totalSupply()).to.be.eq(1_000);
  });

  it("Should be able to transfer via erc20", async function () {
    await (await erc20_contract.transfer(await alice.getAddress(), 150)).wait();
    const zrc2Tokens = await erc20_contract.balanceOf(await alice.getAddress());
    expect(zrc2Tokens).to.be.eq(150);
  });

  it("Should be able properly manipulate allowances", async function () {
    // Allow 50
    await (await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50)).wait();
    let aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(50);

    // Allow 50 more
    await (await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 100)).wait();
    aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(100);

    // Allow 50 fewer
    await (await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50)).wait();
    aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(50);
  });

  it("Should be able to transferFrom via erc20", async function () {
    await (await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50)).wait();
    await (
        await erc20_contract.connect(contractOwner).transferFrom(await alice.getAddress(), await bob.getAddress(), 50)
    ).wait();
    const aliceTokens = await erc20_contract.balanceOf(await alice.getAddress());
    expect(aliceTokens).to.be.eq(100);
    const bobTokens = await erc20_contract.balanceOf(await bob.getAddress());
    expect(bobTokens).to.be.eq(50);
  });

  it("Should be able to transfer to evm contract", async function() {

    expect(await erc20_contract.connect(contractOwner).transfer(erc20_contract.address, 150)).not.to.be.reverted;
    const zrc2Tokens = await erc20_contract.balanceOf(erc20_contract.address);
    expect(zrc2Tokens).to.be.eq(150);
  });

  it("Should not be able to transfer to evm contract when _EvmCall tag is present", async function() {
    expect(erc20_contract.connect(contractOwner).transferFailed(erc20_contract.address, 150)).to.be.reverted;
    const zrc2Tokens = await erc20_contract.balanceOf(erc20_contract.address);
    expect(zrc2Tokens).to.be.eq(150);
  });
});
