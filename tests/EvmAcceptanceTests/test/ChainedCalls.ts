import {assert, expect} from "chai";
import {parallelizer} from "../helpers";
import {ethers} from "hardhat";
import hre from "hardhat";
import {Contract} from "ethers";
import sendJsonRpcRequest from "../helpers/JsonRpcHelper";
import logDebug from "../helpers/DebugHelper";

describe("Chained Contract Calls Functionality", function () {
  //const INITIAL_FUND = 10_000_000;
  let contractOne: Contract;
  let contractTwo: Contract;
  let contractThree: Contract;

  before(async function () {
    contractOne   = await parallelizer.deployContract("ContractOne");
    contractTwo   = await parallelizer.deployContract("ContractTwo");
    contractThree = await parallelizer.deployContract("ContractThree");
  });

  describe("Install and call chained contracts", function () {

    it("Should create a transaction trace after child creation", async function () {
      const METHOD = "debug_traceTransaction";

      let addrOne   = contractOne.address.toLowerCase();
      let addrTwo   = contractTwo.address.toLowerCase();
      let addrThree = contractThree.address.toLowerCase();

      let res = await contractOne.chainedCall([addrTwo, addrThree, addrOne], 0);

      // Now call contract one, passing in the addresses of contracts two and three
      let tracer = {'tracer' : 'callTracer'};

      console.log(res);
      const receipt = await ethers.provider.getTransactionReceipt(res.hash);
      console.log(receipt);

      await sendJsonRpcRequest(METHOD, 1, [res.hash, tracer], (result, status) => {
        logDebug(result);

        assert.equal(status, 200, "has status code");
        let jsonObject = JSON.parse(result.result);

        // Ok, so check that there is 1 call to contract one, 1 to two, 2 to three, 2 to one (via three)
        assert.equal(addrOne, jsonObject["to"].toLowerCase(), "has correct to field for top level call");
        assert.equal(addrTwo, jsonObject["calls"][0]["to"].toLowerCase(), "has correct to field for second level call");

        assert.equal(addrThree, jsonObject["calls"][0]["calls"][0]["to"].toLowerCase(), "has correct to field for third level call (2x)");
        assert.equal(addrThree, jsonObject["calls"][0]["calls"][1]["to"].toLowerCase(), "has correct to field for third level call (2x)");

        assert.equal(addrOne, jsonObject["calls"][0]["calls"][0]["calls"][0]["to"].toLowerCase(), "has correct to field calling back into original contract");
        assert.equal(addrOne, jsonObject["calls"][0]["calls"][1]["calls"][0]["to"].toLowerCase(), "has correct to field calling back into original contract");
      });
    });
  });
});


//RES0:  {
//  type: 'CALL',
//  from: '0xf0cb24ac66ba7375bf9b9c4fa91e208d9eaabd2e',
//  to: '0xa8cae66f62648529eb6ac2f026893fc436107510',
//  value: '0',
//  gas: '0x0',
//  gasUsed: '0x0',
//  input: '12cbb1ad000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000003000000000000000000000000920279cb2096e34b6f7b8c787cb6c8000b13814f0000000000000000000000009fb20310353b553e99904b2e71cddadad76a3612000000000000000000000000a8cae66f62648529eb6ac2f026893fc436107510',
//  output: '0000000000000000000000000000000000000000000000000000000000000000',
//  calls: [
//    {
//      type: 'CALL',
//      from: '0xa8cae66f62648529eb6ac2f026893fc436107510',
//      to: '0x920279cb2096e34b6f7b8c787cb6c8000b13814f',
//      value: '0',
//      gas: '155b0',
//      gasUsed: '0x0',
//      input: '12cbb1ad000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000003000000000000000000000000920279cb2096e34b6f7b8c787cb6c8000b13814f0000000000000000000000009fb20310353b553e99904b2e71cddadad76a3612000000000000000000000000a8cae66f62648529eb6ac2f026893fc436107510',
//      output: '0x0',
//      calls: [Array]
//    }
//  ]
//}
//RES1:  [
//  {
//    type: 'CALL',
//    from: '0xa8cae66f62648529eb6ac2f026893fc436107510',
//    to: '0x920279cb2096e34b6f7b8c787cb6c8000b13814f',
//    value: '0',
//    gas: '155b0',
//    gasUsed: '0x0',
//    input: '12cbb1ad000000000000000000000000000000000000000000000000000000000000004000000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000000003000000000000000000000000920279cb2096e34b6f7b8c787cb6c8000b13814f0000000000000000000000009fb20310353b553e99904b2e71cddadad76a3612000000000000000000000000a8cae66f62648529eb6ac2f026893fc436107510',
//    output: '0x0',
//    calls: [ [Object], [Object] ]
//  }
//]
//RES2:  undefined
//      ✔ Should create a transaction trace after child creation (151ms)
//
//
