const { expect } = require("chai");
const { ethers } = require("hardhat");

// these tests assume that the block_tag is 'latest'
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
    let origin = await contract.getOrigin();
    hre.logDebug("Origin:", origin);
    expect(origin).to.be.eq(owner.address);
  });

  it("Should return the block coinBase when getBlockCoinbase function is called", async function () {
    let blockCoinbase = await contract.getBlockCoinbase();
    hre.logDebug("BlockCoinbase:", blockCoinbase);
    expect(blockCoinbase).to.be.eq("0x0000000000000000000000000000000000000000");
  });

  it("Should return the tx gas price when getTxGasprice function is called", async function () {
    let txGasPrice = await contract.getTxGasprice();
    hre.logDebug("TX gasprice:", txGasPrice);
    expect(txGasPrice).to.be.eq("0x0000000000000000000000000000000000000000");
  });

  it("Should return the block hash for block '0' when getBlockHash function is called", async function () {
    let hash = await contract.getBlockHash(0);
    hre.logDebug("Block hash:", hash);
    assert.isString(hash, "is string");
    assert.match(hash, /^0x/, "should be HEX starting with 0x");
    assert.isNumber(+hash, "can be converted to a number");
  });

  it("Should return the latest block number when getBlockNumber function is called", async function () {
    let blockNumber = await contract.getBlockNumber();
    hre.logDebug("Block number:", blockNumber);
    expect(blockNumber).to.be.gte(0x0);
  });

  it("Should return the latest block difficulty when getBlockDifficulty function is called", async function () {
    let blockDifficulty = await contract.getBlockDifficulty();
    hre.logDebug("Block difficulty:", blockDifficulty);
    expect(blockDifficulty).to.be.gte(0x0);
  });

  it("Should return the block timestamp for latest block when getBlockTimestamp function is called", async function () {
    let timeStamp = await contract.getBlockTimestamp();
    hre.logDebug("Block timestamp:", timeStamp);
    expect(timeStamp).to.be.gt(0x0);
  });

  it("Should return the block gas limit for latest block when getBlockGaslimit function is called", async function () {
    let blockGaslimit = await contract.getBlockGaslimit();
    hre.logDebug("Block Gaslimit :", blockGaslimit);
    expect(blockGaslimit).to.be.gte(0x0);
  });

});
