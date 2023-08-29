import {assert} from "chai";
import sendJsonRpcRequest from "../../helpers/JsonRpcHelper";

const METHOD = "debug_accountRange";
describe(`Otterscan api tests: ${METHOD} #parallel`, function () {

  it("We can get addresses from the node", async function () {

    const PAGE_NUMBER = 0;
    const PAGE_SIZE = 9;

    await sendJsonRpcRequest(METHOD, 1, [PAGE_NUMBER,PAGE_SIZE], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.isNotEmpty(jsonObject.addresses, "Can find addresses of accounts on the node");
    });
  });

  it("We can see constant ordering between requests", async function () {
    const PAGE_NUMBER_1 = 0;
    const PAGE_SIZE_1 = 3;

    let shared_address : string;

    await sendJsonRpcRequest(METHOD, 1, [PAGE_NUMBER_1,PAGE_SIZE_1], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      shared_address = jsonObject.addresses[2];
    });

    const PAGE_NUMBER_2 = 1;
    const PAGE_SIZE_2 = 2;

    await sendJsonRpcRequest(METHOD, 1, [PAGE_NUMBER_2,PAGE_SIZE_2], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.strictEqual(jsonObject.addresses[0], shared_address, "There is consistent ordering of addresses between requests due to overlap");
    });
  });

  it("We can view the first address and that there are more to be seen", async function () {
    const PAGE_NUMBER = 0;
    const PAGE_SIZE = 1;

    await sendJsonRpcRequest(METHOD, 1, [PAGE_NUMBER,PAGE_SIZE], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.isNotEmpty(jsonObject.addresses, "Can find first addresses of accounts");
      assert.isTrue(jsonObject.wasMore, "Can see that there are more addresses to be fetched");
    });
  });

  it("We can reach the end of addresses and see that there are no more to be seen", async function () {
    const PAGE_NUMBER = Number.MAX_SAFE_INTEGER;
    const PAGE_SIZE = 1;

    await sendJsonRpcRequest(METHOD, 1, [PAGE_NUMBER,PAGE_SIZE], (result, status) => {
      assert.equal(status, 200, "has status code");

      let jsonObject = result.result;
      assert.isEmpty(jsonObject.addresses, "Can not find addresses outside range of cache");
      assert.isFalse(jsonObject.wasMore, "Can see that there are no more addresses to be fetched");
    });
  });
});
