const {expect} = require("chai");
const {ethers} = require("hardhat");
const parallelizer = require("../../../helper/Parallelizer");

describe("Openzeppelin role based access control functionality", function () {
  let defaultAdmin, minter, burner;
  let user;

  before(async function () {
    user = ethers.Wallet.createRandom();

    minter = await parallelizer.takeSigner();
    this.contract = await parallelizer.deployContract("OpenZeppelinRoleBasedToken", minter.address);
    defaultAdmin = this.contract.signer;
    burner = await parallelizer.takeSigner();
  });

  after(function () {
    parallelizer.releaseSigner(minter, burner);
  });

  it("should return true if hasRole is called for minter and MINTER_ROLE", async function () {
    const MINTER_ROLE = await this.contract.MINTER_ROLE();
    expect(await this.contract.hasRole(MINTER_ROLE, minter.address)).to.be.true;
  });

  it("should be possible for minter to mint", async function () {
    expect(await this.contract.connect(minter).mint(user.address, 1000)).to.changeTokenBalance(
      this.contract,
      user.address,
      1000
    );

    expect(await this.contract.totalSupply()).to.be.at.least(1000);
  });

  // FIXME: Can't be parallelized yet. Needs ZIL-5055
  xit("should not be possible for non-minter to mint", async function () {
    const account = ethers.Wallet.createRandom();
    await expect(this.contract.mint(account.address, 1000)).to.be.reverted;

    expect(await this.contract.balanceOf(account.address)).to.be.eq(0);
  });

  it("should be possible to grant a role to someone by admin", async function () {
    const BURNER_ROLE = await this.contract.BURNER_ROLE();
    expect(await this.contract.grantRole(BURNER_ROLE, burner.address))
      .to.emit(this.contract, "RoleGranted")
      .withArgs(BURNER_ROLE, burner, defaultAdmin);
  });

  it("should be possible for burner to burn after it grants the access", async function () {
    expect(await this.contract.connect(burner).burn(user.address, 100)).to.changeTokenBalance(
      this.contract,
      user.address,
      -100
    );
  });

  it("should not be possible to grant a role to someone by an arbitrary account", async function () {
    const BURNER_ROLE = await this.contract.BURNER_ROLE();
    let [_, notAdmin] = await ethers.getSigners();
    await expect(this.contract.connect(notAdmin).grantRole(BURNER_ROLE, notAdmin.address)).to.reverted;
  });

  it("should not be possible to revoke a role by an arbitrary account", async function () {
    const BURNER_ROLE = await this.contract.BURNER_ROLE();
    let [, notAdmin] = await ethers.getSigners();
    await expect(this.contract.connect(notAdmin).revokeRole(BURNER_ROLE, burner.address)).to.reverted;
  });

  it("should be possible to revoke a role by admin", async function () {
    const BURNER_ROLE = await this.contract.BURNER_ROLE();
    expect(await this.contract.revokeRole(BURNER_ROLE, burner.address))
      .to.emit(this.contract, "RoleRevoked")
      .withArgs(BURNER_ROLE, burner, defaultAdmin);
  });
});
