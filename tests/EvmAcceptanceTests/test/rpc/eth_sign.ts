import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_sign";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return an error on sending a eth_sign request @block-1", async function () {
    await sendJsonRpcRequest(
      METHOD,
      2,
      ["0xF0C05464f12cB2a011d21dE851108a090C95c755", "0xdeadbeaf"],
      (result, status) => {
        logDebug(result);

        // eth_sign not supported on Zilliqa
        assert.equal(status, 200, "has status code");
        assert.isNumber(result.error.code);
        assert.equal(Number(result.error.code), -32601);
        assert.isString(result.error.message, "is string");
        assert.equal(
          result.error.message,
          "METHOD_NOT_FOUND: The method being requested is not available on this server"
        );
      }
    );
  });
});
