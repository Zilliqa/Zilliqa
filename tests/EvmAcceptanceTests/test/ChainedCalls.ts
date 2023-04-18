import {assert, expect} from "chai";
import {Contract} from "ethers";
import {parallelizer} from "../helpers";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";

describe("Chained Contract Calls Functionality", function () {
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;

  before(async function () {
    contractOne = await parallelizer.deployContract("ContractOne");
    contractTwo = await parallelizer.deployContract("ContractTwo");
    contractThree = await parallelizer.deployContract("ContractThree");
  });

  describe("Install and call chained contracts", function () {
    it("Should create a transaction trace after child creation", async function () {
      const METHOD = "debug_traceTransaction";
      const METHOD_BLOCK = "debug_traceBlockByNumber";

      let addrOne = contractOne.address.toLowerCase();
      let addrTwo = contractTwo.address.toLowerCase();
      let addrThree = contractThree.address.toLowerCase();

      let res = await contractOne.chainedCall([addrTwo, addrThree, addrOne], 0);

      // Now call contract one, passing in the addresses of contracts two and three
      let tracer = {tracer: "callTracer"};

      const receipt = await ethers.provider.getTransactionReceipt(res.hash);

      await sendJsonRpcRequest(METHOD, 1, [res.hash, tracer], (result, status) => {
        assert.equal(status, 200, "has status code");

        let jsonObject = result.result;

        // Ok, so check that there is 1 call to contract one, 1 to two, 2 to three, 2 to one (via three)
        assert.equal(addrOne, jsonObject["to"].toLowerCase(), "has correct to field for top level call");
        assert.equal(addrTwo, jsonObject["calls"][0]["to"].toLowerCase(), "has correct to field for second level call");

        assert.equal(
          addrThree,
          jsonObject["calls"][0]["calls"][0]["to"].toLowerCase(),
          "has correct to field for third level call (2x)"
        );
        assert.equal(
          addrThree,
          jsonObject["calls"][0]["calls"][1]["to"].toLowerCase(),
          "has correct to field for third level call (2x)"
        );

        assert.equal(
          addrOne,
          jsonObject["calls"][0]["calls"][0]["calls"][0]["to"].toLowerCase(),
          "has correct to field calling back into original contract"
        );
        assert.equal(
          addrOne,
          jsonObject["calls"][0]["calls"][1]["calls"][0]["to"].toLowerCase(),
          "has correct to field calling back into original contract"
        );
      });

      let secondTracer = {tracer: "raw"};

      await sendJsonRpcRequest(METHOD, 1, [res.hash, secondTracer], (result, status) => {
        assert.equal(status, 200, "has status code");
      });

      // Query the block by number to get the call
      await sendJsonRpcRequest(METHOD_BLOCK, 1, ["0x" + res.blockNumber.toString(16), tracer], (result, status) => {
        assert.equal(status, 200, "has status code");
        assert.equal(
          addrOne,
          result.result["calls"][0]["to"].toLowerCase(),
          "first call in the result matches the traceTransaction"
        );
      });
    });

    it("Should correctly call chained contracts", async function () {
      let addrOne = contractOne.address.toLowerCase();
      let addrTwo = contractTwo.address.toLowerCase();
      let addrThree = contractThree.address.toLowerCase();

      await expect(contractOne.chainedCall([addrTwo, addrThree, addrOne], 0)).to.emit(contractOne, "FinalMessage");
      await expect(contractOne.chainedCall([addrTwo, addrThree, addrOne], 0)).to.emit(contractTwo, "FinalMessageTwo");
      await expect(contractOne.chainedCall([addrTwo, addrThree, addrOne], 0)).to.emit(
        contractThree,
        "FinalMessageThree"
      );
    });
  });
});
