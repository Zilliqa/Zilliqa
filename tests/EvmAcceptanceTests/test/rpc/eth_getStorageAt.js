const helper = require("../../helper/GeneralHelper");
const zilliqa_helper = require("../../helper/ZilliqaHelper");
assert = require("chai").assert;

var METHOD = "eth_getStorageAt";

describe("Calling " + METHOD, function () {
  let contract;
  before(async function () {
    const Contract = await ethers.getContractFactory("Storage");
    contract = await Contract.deploy();
  });

  it("should return proper storage value when the defaultBlock is 'latest' for storage position 0 at address of contract", async function () {
    await helper.callEthMethod(METHOD, 1, [contract.address, "0x0", "latest"], (result, status) => {
      hre.logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 1024);
    });
  });

  it("should return proper storage value when a value from a map is requested", async function () {
    const MAPPING_SLOT = "0000000000000000000000000000000000000000000000000000000000000001";

    // KEY that we want to read in the mapping
    const KEY = "0000000000000000000000000000000000000000000000000000000000000010";

    // Compute the actual storage slot of the value associated with the key
    const balanceSlot = web3.utils.soliditySha3({t: "bytes", v: KEY + MAPPING_SLOT});

    await helper.callEthMethod(METHOD, 1, [contract.address, balanceSlot, "latest"], (result, status) => {
      hre.logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 2048);
    });
  });

  it("should return proper storage value when the defaultBlock is '0x0' for storage position 0 at address of contract", async function () {
    await helper.callEthMethod(METHOD, 1, [contract.address, "0x0", "0x0"], (result, status) => {
      hre.logDebug("Result:", result);
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isString(result.result, "is string");
      assert.match(result.result, /^0x/, "should be HEX starting with 0x");

      assert.equal(web3.utils.hexToNumber(result.result), 1024);
    });
  });

  it("should return an error when no parameter is passed", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      assert.equal(status, 200, "has status code");
      assert.property(result, "error");
      assert.equal(result.error.code, -32602);
    });
  });
});
