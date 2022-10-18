const { expect } = require("chai");
const { ethers } = require("hardhat");

describe("Blockchain information for smart contracts", function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("BlockchainInstructions");
    contract = await Contract.deploy();
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).exist;
  });

  it("Should return the owner address when getOrigin function is called", async function () {
    const [owner] = await ethers.getSigners();
    expect(await contract.getOrigin()).to.be.eq(owner.address);
  });

  it("Should return the block coinBase when getBlockCoinbase function is called", async function () {
    expect(await contract.getBlockCoinbase()).to.be.eq("0x0000000000000000000000000000000000000000");
  });

  it("Should return the tx gas price when getTxGasprice function is called", async function () {
    expect(await contract.getTxGasprice()).to.be.eq("0x0000000000000000000000000000000000000000");
  });

  it("Should return the block hash for block '0' when getBlockHash function is called", async function () {
    let hash = await contract.getBlockHash(0);
    hre.logDebug(hash);
    assert.isString(hash, "is string");
    assert.match(hash, /^0x/, "should be HEX starting with 0x");
    assert.isNumber(+hash, "can be converted to a number");
  });

  it("Should return the latest block number when getBlockNumber function is called", async function () {
    let blockNumber = await contract.getBlockNumber();
    hre.logDebug(blockNumber.value);
    assert.isString(blockNumber.value, "is not a string");
    //assert.match(blockNumber, /^0x/, "should be HEX starting with 0x");
    //assert.isNumber(+blockNumber, "can be converted to a number");
  });

  it("Should return the latest block difficulty when getBlockDifficulty function is called", async function () {
    let blockDifficulty = await contract.getBlockDifficulty();//   ??? expect(await contract.getDifficulty()).to.be.lt(0x0);
    hre.logDebug(blockDifficulty);
    assert.isString(blockDifficulty.value, "is not a string");
    //assert.match(blockDifficulty, /^0x/, "should be HEX starting with 0x");
    //assert.isNumber(+blockDifficulty, "can be converted to a number");
  });

  it("Should return the block timestamp for latest block when getBlockTimestamp function is called", async function () {
    expect(await contract.getBlockTimestamp()).to.be.gt(0x0);
  });

  it("Should return the block gas limit for latest block when getBlockGaslimit function is called", async function () {
    expect(await contract.getBlockGaslimit()).to.be.gte(0x0);
  });

});
