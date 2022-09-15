const helper = require('../../helper/GeneralHelper');
const { ZilliqaHelper } = require('../../helper/ZilliqaHelper');
const { ethers, web3 } = require("hardhat")
assert = require('chai').assert;

const METHOD = 'eth_getBalance';
const initialBalance = 10 * (10 ** 6);
let receiverAccount;

describe("Calling " + METHOD, async function () {
    let zHelper = new ZilliqaHelper();
    this.beforeEach("Initialize unique receiver account with balance from sender account", async function () {

    });

    describe("When tag is 'latest'", async function () {
        it("should return the latest balance as specified in the ethereum protocol", async function () {
            // check sender account has enough balance to move
            const senderAddress = zHelper.getPrimaryAccount().address;
            console.log("Primary address:", senderAddress);

            receiverAddress = zHelper.getSecondaryAccount().address;
            console.log("New Account address:", receiverAddress);

            const senderBalance = await web3.eth.getBalance(senderAddress);
            console.log("Sender balance:", senderBalance);
            assert.isAbove(ethers.BigNumber.from(senderBalance), initialBalance, 'sender has enough balance to move');

            const receiverBalance = await web3.eth.getBalance(receiverAddress);
            console.log("Receiver balance:", receiverBalance);

            var transactionHash;
            function onMoveFundsFinished(receipt) {
                transactionHash = receipt.transactionHash;
                console.log("Then finished, tx hash:", receipt.transactionHash);
                web3.eth.getTransaction(receipt.transactionHash).then(console.log);
            };

            function onMoveFundsError(error) {
                console.log("Then with Error:", error);
            };

            await zHelper.moveFunds(initialBalance, receiverAddress).then(onMoveFundsFinished, onMoveFundsError);
            //var done = false;
            //while (!done) {
            //    console.log(transactionHash);
            //    await web3.eth.getTransaction(transactionHash).then(function (receipt) {
            //        console.log("GetTransaction:", receipt.hash);
            //        if (receipt.hash != "") {
            //            done = true
            //        }
            //    });
            //
            //    setTimeout(() => { console.log("Timeout...") }, 1000);
            //}

            const newReceiverBalance = await web3.eth.getBalance(receiverAddress);
            console.log("Delta Receiver balance:", receiverBalance - newReceiverBalance);

            console.log("Requesting balance for account:", receiverAddress);
            await helper.callEthMethod(METHOD, 1, [
                receiverAddress,
                "latest"],
                (result, status) => {
                    console.log("Result:", result);

                    assert.equal(status, 200, 'has status code');
                    assert.property(result, 'result', (result.error) ? result.error.message : 'error');
                    assert.isString(result.result, 'is string');
                    assert.match(result.result, /^0x/, 'should be HEX starting with 0x');
                    assert.isNumber(+result.result, 'can be converted to a number');

                    var expectedBalance = ethers.BigNumber.from(receiverBalance + initialBalance);
                    console.log("Initial balance:", initialBalance);
                    console.log("Sender balance:", senderBalance);
                    console.log("Receiver balance:", receiverBalance);
                    console.log("New Receiver balance:", newReceiverBalance);

                    assert.equal(result.result, expectedBalance.toHexString(), 'Has result:' + result + ' should have balance ' + expectedBalance.toHexString());
                })
        })
    })

    describe.skip("When tag is 'earliest'", async function () {
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

    describe.skip("When tag is 'pending'", function () {
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