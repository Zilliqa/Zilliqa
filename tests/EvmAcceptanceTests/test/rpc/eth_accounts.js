const helper = require("../../helper/GeneralHelper");
const {assert} = require("chai");

const METHOD = "eth_accounts";

describe("Calling " + METHOD, function () {
  it("should return an array with accounts", async function () {
    await helper.callEthMethod(METHOD, 1, [], (result, status) => {
      hre.logDebug(result);

      // block number changes at every call, so check only the response format
      assert.equal(status, 200, "has status code");
      assert.property(result, "result", result.error ? result.error.message : "error");
      assert.isArray(result.result, "is array");
    });
  });
});
