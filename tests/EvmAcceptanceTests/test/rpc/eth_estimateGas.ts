import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";
import {assert} from "chai";

const METHOD = "eth_estimateGas";

// TODO, finish test when  code on zilliqa is implemented to calculate the estimated gas price

// describe("Calling " + METHOD, function () {
//   it("should return the estimated gas as calculated over the transaction provided", async function () {
//     await sendJsonRpcRequest(METHOD, 2, [
//       "{\"from\":\"\", \"to\":\"\", \"value\":\"\", \"gas\":\"\", \"data\":\"\"}",
//       "latest"],
//       (result, status) => {
//         console.log(result);
//
//         assert.equal(status, 200, 'has status code');
//         assert.property(result, 'result', (result.error) ? result.error.message : 'error');
//         assert.isString(result.result, 'is string');
//         assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
//         assert.isNumber(+result.result, 'can be converted to a number');
//
//         const estimatedGas = 2000000000
//         assert.equal(+result.result, estimatedGas, 'should have an estimated gas' + estimatedGas);
//       })
//   })
// })
