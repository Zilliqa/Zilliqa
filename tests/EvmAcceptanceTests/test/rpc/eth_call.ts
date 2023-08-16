import callJsonRpc from "../../helpers/JsonRpcHelper";
import {assert} from "chai";
import {ethers} from "hardhat";
import logDebug from "../../helpers/DebugHelper";

const METHOD = "eth_call";

describe("Calling " + METHOD, function () {
  it("should return the from eth call", async function () {
    const contractFactory = await ethers.getContractFactory("SimpleContract");
    const [signer] = await ethers.getSigners();

    await callJsonRpc(
      METHOD,
      2,
      [
        {
          to: signer.address,
          data: contractFactory.bytecode,
          gas: 1000000
        },
        "latest"
      ],
      (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        assert.property(result, "result", result.error ? result.error.message : "error");
        assert.isString(result.result, "is string");
        assert.match(result.result, /^0x/, "should be HEX starting with 0x");
        assert.isNumber(+result.result, "can be converted to a number");
        assert.equal(result.result, "0x");
      }
    );
  });
});
