import {expect} from "chai";
import {Contract, Signer} from "ethers";
import hre from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";

describe.skip("ERC20 Interop", function () {
  let zrc2_contract: ScillaContract;
  let bridge_contract: Contract;
  let contractOwner: Signer;
  let alice: Signer;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contractOwner = await parallelizer.takeSigner();

    zrc2_contract = await parallelizer.deployScillaContract(
      "ZRC2Interop",
      await contractOwner.getAddress(),
      "ZRC2Interop Token",
      "SDT",
      2,
      1_000
    );
    alice = await parallelizer.takeSigner();
    bridge_contract = await parallelizer.deployContractWithSigner(
      contractOwner,
      "ERC20Interop",
      zrc2_contract.address?.toLowerCase()
    );
  });

  it("Interop Should be deployed successfully", async function () {
    expect(zrc2_contract.address).to.be.properAddress;
    expect(bridge_contract.address).to.be.properAddress;
  });

  it("Interop Should return correct contract owner from ZRC2", async function () {
    expect(await zrc2_contract.contract_owner()).to.be.eq(await contractOwner.getAddress());
  });

  it("Should return zrc2/erc20 total supply via bridge contract", async function () {
    expect(await bridge_contract.totalSupplyZRC2()).to.be.eq(1_000);
    expect(await bridge_contract.totalSupplyERC20()).to.be.eq(0);
  });

  it("Should be able to mint token to zrc2 contract by contract owner", async function () {
    await (await bridge_contract.mintZRC2(await alice.getAddress(), 100)).wait();
    const aliceTokens = await bridge_contract.balanceOfZRC2(await alice.getAddress());
    expect(aliceTokens).to.be.eq(100);
    expect(await bridge_contract.totalSupplyZRC2()).to.be.eq(1100);
  });

  it("Should be able to transfer from zrc2 to erc20", async function () {
    await (await bridge_contract.transferZrc2ToErc20(await alice.getAddress(), 50)).wait();
    const zrc2Tokens = await bridge_contract.balanceOfZRC2(await alice.getAddress());
    expect(zrc2Tokens).to.be.eq(50);
    const erc20Tokens = await bridge_contract.balanceOfERC20(await alice.getAddress());
    expect(erc20Tokens).to.be.eq(50);
  });

  it("Should be able to transfer from erc20 to zrc2", async function () {
    await (await bridge_contract.transferErc20ToZrc2(await alice.getAddress(), 25)).wait();
    const zrc2Tokens = await bridge_contract.balanceOfZRC2(await alice.getAddress());
    expect(zrc2Tokens).to.be.eq(75);
    const erc20Tokens = await bridge_contract.balanceOfERC20(await alice.getAddress());
    expect(erc20Tokens).to.be.eq(25);
  });

  it("Should be possible to burn tokens by owner", async function () {
    await (await bridge_contract.burnZRC2(await alice.getAddress(), 25)).wait();
    const aliceTokens = await bridge_contract.balanceOfZRC2(await alice.getAddress());
    expect(aliceTokens).to.be.eq(50);
  });

  it("Should not be possible to mint or burn tokens by contract non-owner", async function () {
    expect (bridge_contract.connect(alice).burnZRC2(await alice.getAddress(), 50)).to.be.reverted;
    let aliceTokens = await bridge_contract.connect(alice).balanceOfZRC2(await alice.getAddress());
    expect(aliceTokens).to.be.eq(50);

    expect(bridge_contract.connect(alice).mintZRC2(await alice.getAddress(), 50)).to.be.reverted;
    aliceTokens = await bridge_contract.connect(alice).balanceOfZRC2(await alice.getAddress());
    expect(aliceTokens).to.be.eq(50);
  });

  it("Should be possible to do bridge transfers only by contract owner", async function () {
    expect(bridge_contract.connect(alice).transferErc20ToZrc2(await alice.getAddress(), 50)).to.be.reverted;
    const zrc2Tokens = await bridge_contract.balanceOfZRC2(await alice.getAddress());
    expect(zrc2Tokens).to.be.eq(50);

    expect(bridge_contract.connect(alice).transferZrc2ToErc20(await alice.getAddress(), 50)).to.be.reverted;
    const erc20Tokens = await bridge_contract.balanceOfERC20(await alice.getAddress());
    expect(erc20Tokens).to.be.eq(25);
  });
});
