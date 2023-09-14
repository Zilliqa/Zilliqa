import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_getUncleByBlockHashAndIndex";

describe(`Calling ${METHOD} #parallel`, function () {
  it("should return the uncle Id by block hash @block-1", async function () {
    await sendJsonRpcRequest(
      METHOD,
      2,
      ["0xc6ef2fc5426d6ad6fd9e2a26abeab0aa2411b7ab17f30a99d3cb96aed1d1055b", "0x0"],
      (result, status) => {
        logDebug(result);
        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isNumber(+result.result, "can be converted to a number");

        assert.equal(result.result, null, "should have a uncle null");
      }
    );
  });
});
