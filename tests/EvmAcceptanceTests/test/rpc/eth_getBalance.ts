import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";
import hre from "hardhat";

const METHOD = "eth_getBalance";

describe("Calling " + METHOD, function () {
  describe("When tag is 'latest'", function () {
    it("should return the latest balance from the specified account", async function () {
      const [signer] = await ethers.getSigners();
      await sendJsonRpcRequest(METHOD, 1, [signer.address, "latest"], (result, status) => {
        logDebug("Result:", result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");

        var expectedBalance = 0;
        assert.isAbove(
          +result.result,
          expectedBalance,
          "Has result:" + result + " should have balance " + expectedBalance
        );
      });
    });
  });

  describe("When tag is 'earliest'", function () {
    describe("When on Zilliqa network", function () {
      before(async function () {
        if (!hre.isZilliqaNetworkSelected()) {
          this.skip();
        }
      });

      it("should return the earliest balance as specified in the ethereum protocol", async function () {
        const [signer] = await ethers.getSigners();
        await sendJsonRpcRequest(METHOD, 1, [signer.address, "earliest"], (result, status) => {
          logDebug("Result:", result);

          assert.equal(status, 200, "has status code");
          assert.property(result, "result", result.error ? result.error.message : "error");
          assert.isString(result.result, "is string");
          assert.match(result.result, /^0x/, "should be HEX starting with 0x");
          assert.isNumber(+result.result, "can be converted to a number");

          var expectedBalance = 0;
          assert.isAbove(
            +result.result,
            expectedBalance,
            "Has result:" + result + " should have balance " + expectedBalance
          );
        });
      });
    });
  });

  describe("When tag is 'pending'", function () {
    describe("When on Zilliqa network", function () {
      before(async function () {
        if (!hre.isZilliqaNetworkSelected()) {
          this.skip();
        }
      });

      it("should return the pending balance as specified in the ethereum protocol", async function () {
        const [signer] = await ethers.getSigners();
        await sendJsonRpcRequest(METHOD, 1, [signer.address, "pending"], (result, status) => {
          logDebug("Result:", result);

          assert.equal(status, 200, "has status code");
          assert.property(result, "result", result.error ? result.error.message : "error");
          assert.isString(result.result, "is string");
          assert.match(result.result, /^0x/, "should be HEX starting with 0x");
          assert.isNumber(+result.result, "can be converted to a number");

          var expectedBalance = 0;
          assert.isAbove(
            +result.result,
            expectedBalance,
            "Has result:" + result + " should have balance " + expectedBalance
          );
        });
      });
    });
  });

  describe("When tag is 'unknown tag'", function () {
    let expectedErrorMessage = "";
    let errorCode = 0;
    before(async function () {
      if (hre.isZilliqaNetworkSelected()) {
        expectedErrorMessage = "Unable To Process, invalid tag";
        errorCode = -1;
      } else {
        expectedErrorMessage =
          "Cannot wrap string value 'unknown tag' as a json-rpc type; strings must be prefixed with '0x'.";
        errorCode = -32700;
      }
    });

    it("should return an error requesting the balance due to invalid tag", async function () {
      const [signer] = await ethers.getSigners();
      await sendJsonRpcRequest(
        METHOD,
        1,
        [signer.address, "unknown tag"], // not supported tag should give an error
        (result, status) => {
          logDebug(result);

          assert.equal(status, 200, "has status code");
          assert.equal(result.error.code, errorCode);
          assert.equal(result.error.message, expectedErrorMessage);
        }
      );
    });
  });
});
