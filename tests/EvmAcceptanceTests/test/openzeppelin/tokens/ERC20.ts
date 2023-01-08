import {expect} from "chai";
import {Contract} from "ethers";
import {ethers} from "hardhat";

describe("Openzeppelin ERC20 functionality", function () {
  const TOTAL_SUPPLY = 1_000_000;
  let contract: Contract;

  before(async function () {
    const Contract = await ethers.getContractFactory("OpenZeppelinGLDToken");
    contract = await Contract.deploy(TOTAL_SUPPLY);
    await contract.deployed();
  });

  describe("General", function () {
    it("Should return 1_000_000 as the total supply of the token", async function () {
      expect(await contract.totalSupply()).to.be.equal(TOTAL_SUPPLY);
    });

    it("Should return 1_000_000 for the owner's balance at the beginning", async function () {
      const [owner] = await ethers.getSigners();
      expect(await contract.balanceOf(owner.address)).to.eq(TOTAL_SUPPLY);
    });
  });

  describe("Transfer", function () {
    it("Should be possible to transfer GLD token from the owner to another [@transactional]", async function () {
      const [owner, receiver] = await ethers.getSigners();

      expect(await contract.transfer(receiver.address, 1000))
        .to.changeTokenBalances(contract, [receiver.address, owner.address], [1000, -1000])
        .to.emit(contract, "Transfer")
        .withArgs(owner.address, receiver.address, 1000);
    });

    it("Should not be possible to transfer GLD token by an arbitrary account", async function () {
      const [, , notOwner] = await ethers.getSigners();

      await expect(contract.connect(notOwner).transfer(notOwner.address, 1000)).to.be.revertedWith(
        "ERC20: transfer amount exceeds balance"
      );
      expect(await contract.balanceOf(notOwner.address)).to.be.eq(0);
    });

    it("Should not be possible to move more than available tokens to some address", async function () {
      const [_, receiver] = await ethers.getSigners();
      const totalSupply = await contract.totalSupply();

      // Move one coin more than available coins.
      await expect(contract.transfer(receiver.address, totalSupply + 1)).to.be.revertedWith(
        "ERC20: transfer amount exceeds balance"
      );
    });
  });

  describe("Transfer From", function () {
    it("Should not be possible to transfer from one account to another if allowance is insufficient", async function () {
      const [_, sender, spender] = await ethers.getSigners();

      // Fund the 2nd account first
      await contract.transfer(sender.address, 5_000);

      await expect(contract.transferFrom(sender.address, spender.address, 2_999)).to.be.revertedWith(
        "ERC20: insufficient allowance"
      );
    });

    it("Should be possible to transfer from one account to another if allowance is ok[@transactional]", async function () {
      const [owner, sender, spender] = await ethers.getSigners();

      // Fund the 2nd account first
      const transferPromise = contract.transfer(sender.address, 5_000);
      const approvePromise = contract.connect(sender).approve(owner.address, 2000);

      // Let's wait for them in parallel to reduce execution time.
      await Promise.all([transferPromise, approvePromise]);

      expect(await contract.transferFrom(sender.address, spender.address, 1_999))
        .to.changeTokenBalances(contract, [sender.address, spender.address], [1_999, -1_999])
        .emit(contract, "Transfer")
        .withArgs(sender.address, spender.address, 1_999);

      expect(await contract.allowance(sender.address, owner.address)).to.be.eq(1);
    });
  });
});
