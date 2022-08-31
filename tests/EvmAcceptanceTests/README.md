# Quick Start

```bash
    npm install
    npx hardhat test    # to run tests
```

# Start Testing
## Add a new contract
1. Add a new contract to `contracts` folder
2. Compile it using `npx hardhat compile`

## Add a new test scenario
1. Add a new javascript file to `test` folder
2. Deploy the contract.
3. Add your test cases using `describe` and `it` blocks
4. Expect results using [chai assertions](https://www.chaijs.com/api/bdd/). Below is the list of most useful ones:
```javascript
expect(123).to.equal(123)
expect(2).to.not.equal(1);
expect(true).to.be.true;
expect(false).to.be.false;
expect(null).to.be.null;
expect(undefined).to.be.undefined;
expect(contract.address).exist;
expect('foobar').to.have.string('bar');
expect(badFn).to.throw();
```
## Run the tests
```bash
npx hardhat test
npx hardhat test --grep "something"
npx hardhat test --bail     # Stop running tests after the first test failure
npx hardhat test --parallel
```


# How to define a new network for hardhat
1. Add a new network to `hardhat.config.js` inside `networks` property:
```json
...
module.exports = {
  solidity: "0.8.9",
  defaultNetwork: "isolated_server",
  networks: {
    ganache: {
      url: "http://127.0.0.1:7545",
      chainId: 1337,
      accounts: [
        "c95690aed4461afd835b17492ff889af72267a8bdf7d781e305576cd8f7eb182",
        "05751249685e856287c2b2b9346e70a70e1d750bc69a35cef740f409ad0264ad"
      ]
    },
...
```

2. Change the default network:
```json
module.exports = {
  solidity: "0.8.9",
  defaultNetwork: "ganache",
...
```
Alternatively, It's possible to run the tests with `--network` option:
```bash
npx hardhat test --network ganache
```
# Testing conventions and best practices
* File names tries to tell us the scenario we're testing.
* Every `it` block should have one assertion, readable and easy to understand.
* Follow `GIVEN`, `WHEN` and, `THEN` BDD style
    * Top level `describe` for `GIVEN`
    * Nested `describe`s for `WHEN`
    * `it` blocks for `THEN`

```javascript
// GIVEN 
describe("Contract with payable constructor", function () {
    // WHEN
    describe("When ethers.js is used", function () {
        let contract;
        let INITIAL_BALANCE = 10;

        before(async function () {
            const Contract = await ethers.getContractFactory("WithPayableConstructor")
            contract = await Contract.deploy({
                value: INITIAL_BALANCE
            })
        })

        // THEN
        it("Should be deployed successfully", async function () {
            expect(contract.address).exist;
        })
    });
});

```