import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import hre from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_sendTransaction";

describe("Calling " + METHOD, function () {
  describe("When on Zilliqa network", function () {
    before(async function () {
      if (!hre.isZilliqaNetworkSelected()) {
        this.skip();
      }
    });

    it("Should return a an error on sending send transaction request", async function () {
      await sendJsonRpcRequest(
        METHOD,
        1,
        [
          {
            data: "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675",
            from: "0xF0C05464f12cB2a011d21dE851108a090C95c755",
            gas: "",
            gasPrice: "",
            to: "0xd46e8dd67c5d32be8058bb8eb970870f07244567",
            value: "0x9184e72a"
          }
        ],
        (result, status) => {
          logDebug(result);

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
});
