const helper = require('../../helper/GeneralHelper');
const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
const { ethers, web3 } = require("hardhat")
assert = require('chai').assert;

const METHOD = 'eth_getBalance';
const initialBalance = 10_000;
let receiverAccount;

describe("Calling " + METHOD, async function () {
    this.beforeEach("Initialize unique receiver account with balance from sender account", async function () {
        let zHelper = new ZilliqaHelper();
        // check sender account has enough balance to move
        const senderAddress = zHelper.getPrimaryAccountAddress();
        console.log("Primary address:", senderAddress);
        const senderBalance = await web3.eth.getBalance(senderAddress);
        console.log("Sender balance:", senderBalance);
        assert.isAbove(ethers.BigNumber.from(senderBalance), initialBalance, 'sender has enough balance to move');

        // create unique receiver account
        receiverAccount = await web3.eth.accounts.create();
        console.log("New Account address:", receiverAccount.address);

        await zHelper.moveFunds(initialBalance, receiverAccount.address);
    });

    describe("When tag is 'latest'", async function () {
        it("should return the latest balance as specified in the ethereum protocol", async function () {
            console.log("Requesting balance for account:", receiverAccount.address);
            await helper.callEthMethod(METHOD, 1, [
                receiverAccount.address, // public address
                "latest"],
                (result, status) => {
                    console.log(result);

                    assert.equal(status, 200, 'has status code');
                    assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                    assert.isString(result.result, 'is string');
                    assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                    assert.isNumber(+result.result, 'can be converted to a number');
                    assert.equal(+result.result, initialBalance, 'Has result:' + result + ' should have balance ' + initialBalance);
                })
        })
    })

    describe("When tag is 'earliest'", async function () {
        describe("When on Zilliqa network", async function () {
            before(async function () {
                if (!helper.isZilliqaNetworkSelected()) {
                    this.skip();
                }
            });

            it("should return the earliest balance as specified in the ethereum protocol", async function () {
                await helper.callEthMethod(METHOD, 1, [
                    receiverAccount.address, // public address
                    "earliest"],
                    (result, status) => {
                        console.log(result);
                        assert.equal(status, 200, 'has status code');
                        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                        assert.isString(result.result, 'is string');
                        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                        assert.isNumber(+result.result, 'can be converted to a number');

                        assert.equal(+result.result, initialBalance, 'Has result:' + result + ' should have balance ' + initialBalance);
                    })
            })
        })
    })

    describe("When tag is 'pending'", function () {
        describe("When on Zilliqa network", async function () {
            before(async function () {
                if (!helper.isZilliqaNetworkSelected()) {
                    this.skip();
                }
            });

            it("should return the pending balance as specified in the ethereum protocol", async function () {
                await helper.callEthMethod(METHOD, 1, [
                    receiverAccount.address, // public address
                    "pending"],
                    (result, status) => {
                        console.log(result);
                        assert.equal(status, 200, 'has status code');
                        assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                        assert.isString(result.result, 'is string');
                        assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                        assert.isNumber(+result.result, 'can be converted to a number');

                        assert.equal(+result.result, initialBalance, 'Has result:' + result + ' should have balance ' + initialBalance);
                    })
            })
        })
    })

    //    describe("When getBalance tag is invalid tag", function () {
    //        let expectedErrorMessage = ""
    //        before(async function () {
    //            if (helper.isZilliqaNetworkSelected()) {
    //                expectedErrorMessage = "Unable To Process, invalid tag"
    //            }
    //            else {
    //                expectedErrorMessage = "Cannot wrap string value \"unknown tag\" as a json-rpc type; strings must be prefixed with \"0x\"."
    //            }
    //        })
    //
    //        it("should return an error requesting the balance due to invalid tag", async function () {
    //            await helper.callEthMethod(METHOD, 1, [
    //                "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",   // public address
    //                "unknown tag"],                                 // not supported tag should give an error
    //                (result, status) => {
    //                    assert.equal(status, 200, 'has status code');
    //                    assert.equal(result.error.code, -32700);
    //                    assert.equal(result.error.message, expectedErrorMessage);
    //                })
    //        })
    //    })
})