import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import logDebug from "../../helpers/DebugHelper";
import hre from "hardhat";

const METHOD = "eth_coinbase";

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", function () {
    before(async function () {
      if (!hre.isZilliqaNetworkSelected()) {
        this.skip();
      }
    });

    it("should return an error on eth_coinbase", async function () {
      await sendJsonRpcRequest(METHOD, 1, [], (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.isNumber(result.error.code);
        assert.equal(Number(result.error.code), -32600);
        assert.isString(result.error.message, "is string");
      });
    });
  });
});
