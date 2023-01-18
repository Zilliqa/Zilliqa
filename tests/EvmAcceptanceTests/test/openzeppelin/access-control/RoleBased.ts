import {expect} from "chai";
import {parallelizer} from "../../../helpers";
import hre, {ethers} from "hardhat";
import {Contract, Signer, Wallet} from "ethers";

describe("Openzeppelin role based access control functionality", function () {
  let defaultAdmin: Signer;
  let minter: Wallet;
  let burner: Wallet;
  let user: Wallet;
  let contract: Contract;

  before(async function () {
    user = ethers.Wallet.createRandom();

    minter = await parallelizer.takeSigner();
    contract = await parallelizer.deployContract("OpenZeppelinRoleBasedToken", minter.address);
    defaultAdmin = contract.signer;
    burner = await parallelizer.takeSigner();
  });

  after(function () {
    parallelizer.releaseSigner(minter, burner);
  });

  it("should return true if hasRole is called for minter and MINTER_ROLE", async function () {
    const MINTER_ROLE = await contract.MINTER_ROLE();
    expect(await contract.hasRole(MINTER_ROLE, minter.address)).to.be.true;
  });

  it("should be possible for minter to mint", async function () {
    expect(await contract.connect(minter).mint(user.address, 1000)).to.changeTokenBalance(contract, user.address, 1000);

    expect(await contract.totalSupply()).to.be.at.least(1000);
  });

  // FIXME: Can't be parallelized yet. Needs ZIL-5055
  xit("should not be possible for non-minter to mint", async function () {
    const account = ethers.Wallet.createRandom();
    await expect(contract.mint(account.address, 1000)).to.be.reverted;

    expect(await contract.balanceOf(account.address)).to.be.eq(0);
  });

  it("should be possible to grant a role to someone by admin", async function () {
    const BURNER_ROLE = await contract.BURNER_ROLE();
    expect(await contract.grantRole(BURNER_ROLE, burner.address))
      .to.emit(contract, "RoleGranted")
      .withArgs(BURNER_ROLE, burner, defaultAdmin);
  });

  it("should be possible for burner to burn after it grants the access", async function () {
    expect(await contract.connect(burner).burn(user.address, 100)).to.changeTokenBalance(contract, user.address, -100);
  });

  it("should not be possible to grant a role to someone by an arbitrary account", async function () {
    const BURNER_ROLE = await contract.BURNER_ROLE();
    let [_, notAdmin] = await ethers.getSigners();
    await expect(contract.connect(notAdmin).grantRole(BURNER_ROLE, notAdmin.address)).to.reverted;
  });

  it("should not be possible to revoke a role by an arbitrary account", async function () {
    const BURNER_ROLE = await contract.BURNER_ROLE();
    let [, notAdmin] = await ethers.getSigners();
    await expect(contract.connect(notAdmin).revokeRole(BURNER_ROLE, burner.address)).to.reverted;
  });

  it("should be possible to revoke a role by admin", async function () {
    const BURNER_ROLE = await contract.BURNER_ROLE();
    expect(await contract.revokeRole(BURNER_ROLE, burner.address))
      .to.emit(contract, "RoleRevoked")
      .withArgs(BURNER_ROLE, burner, defaultAdmin);
  });
});
