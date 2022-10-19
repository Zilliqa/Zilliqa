const {expect} = require("chai");
const {ethers} = require("hardhat");

describe("ERC20 functionality", function () {
  const TOTAL_SUPPLY = 1_000_000;
  let contract;
  before(async function () {
    // The first three address needs to have funded with some zils
    const [account1, account2, account3] = await ethers.getSigners();
    const firstBalance = await ethers.provider.getBalance(account1.address);
    const secondBalance = await ethers.provider.getBalance(account2.address);
    const thirdBalance = await ethers.provider.getBalance(account3.address);
    if (firstBalance == 0 || secondBalance == 0 || thirdBalance == 0) {
      throw new Error("The first three accounts need to have some zils to run these tests.");
    }

    const Contract = await ethers.getContractFactory("ZEVMToken");
    contract = await Contract.deploy("ZERC", "Zilliqa ERC Token", TOTAL_SUPPLY);
  });

  describe("General", function () {
    it("Should return ZERC as the symbol of the token", async function () {
      expect(await contract.symbol()).to.be.equal("ZERC");
    });

    it("Should return 1_000_000 as the total supply of the token", async function () {
      expect(await contract.totalSupply()).to.be.equal(TOTAL_SUPPLY);
    });

    it("Should return 1_000_000 for the owner's balance at the beginning", async function () {
      const [owner] = await ethers.getSigners();
      expect(await contract.balanceOf(owner.address)).to.eq(TOTAL_SUPPLY);
    });
  });

  describe("Transfer", function () {
    it("Should be possible to transfer ZERC from the owner to another [@transactional]", async function () {
      const [owner, receiver] = await ethers.getSigners();
      const prevBalance = await contract.balanceOf(owner.address);

      await contract.transfer(receiver.address, 1000);

      expect(await contract.balanceOf(receiver.address)).to.eq(1000);
      expect(await contract.balanceOf(owner.address)).to.eq(prevBalance - 1000);
    });
    // TODO: Add event related test(s), e.g. for Transfer event

    it("Should not be possible to move more than available tokens to some address [@transactional]", async function () {
      const [owner, receiver] = await ethers.getSigners();
      const prevBalance = await contract.balanceOf(owner.address);

      // Move one coin more than available coins.
      // Workaround below since for some reason chai assertions don't handle exceptions properly,
      // e.g. expect(cond).to.throw() doesn't work here
      try {
        await contract.transfer(receiver.address, prevBalance + 1);
        expect(false).to.be(true);
      } catch (_) {}
      expect(await contract.balanceOf(owner.address)).to.eq(prevBalance);
    });
  });

  describe("Approve", function () {
    it("Should return 0 if the recipient account doesn't have the allowance to receive token", async function () {
      const [owner, receiver] = await ethers.getSigners();
      expect(await contract.allowance(owner.address, receiver.address)).to.be.eq(0);
    });

    it("Should be possible to approve [@transactional]", async function () {
      const [sender, spender] = await ethers.getSigners();

      await contract.approve(spender.address, 50_000);
      expect(await contract.allowance(sender.address, spender.address)).to.eq(50_000);
    });
    // TODO: Add event related test(s), e.g. for Approval event
  });

  describe("Transfer From", function () {
    it("Should be possible to transfer from one account to another [@transactional]", async function () {
      const [owner, _, sender, spender] = await ethers.getSigners();

      // Fund the 2nd account first
      await contract.transfer(sender.address, 5_000);

      // Approve 30_000 for withdrawal by Acc #3
      await contract.connect(sender).approve(spender.address, 3_000);

      // Try transferring allowed amount
      await contract.transferFrom(sender.address, spender.address, 2_999);

      expect(await contract.balanceOf(sender.address)).to.be.eq(2_001);
      expect(await contract.allowance(sender.address, spender.address)).to.be.eq(1);
    });
  });
});
