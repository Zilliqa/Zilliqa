# Quick Start

```bash
    npm install
    npx hardhat test    # to run tests
    npx hardhat test --network devnet    # to run tests against the devnet
    npx hardhat test --log-jsonrpc    # to run tests and print JSON-RPC requests/responses
    npx hardhat test --debug    # to run tests and print log messages
    npx hardhat test --grep something    # to run tests containing `something` in the description
    npx hardhat test filename    # to run tests of `filename`
    npx hardhat test folder/*    # to run tests of `folder`
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
expect(123).to.equal(123);
expect(2).to.not.equal(1);
expect(true).to.be.true;
expect(false).to.be.false;
expect(null).to.be.null;
expect(undefined).to.be.undefined;
expect(contract.address).exist;
expect("foobar").to.have.string("bar");
expect(badFn).to.throw();
```

It's also useful to use [hardhat chai matchers](https://hardhat.org/hardhat-chai-matchers/docs/overview) if possible:

```javascript
await expect(contract.call()).to.emit(contract, "Uint").withArgs(3); // For events
await expect(contract.call()).to.be.reverted;
await expect(contract.call()).to.be.revertedWith("Some revert message");
expect("0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266").to.be.a.properAddress;
await expect(contract.withdraw())
  .to.changeEtherBalance(contract.address, ethers.utils.parseEther("-1.0"))
  .to.changeEtherBalance(owner.address, ethers.utils.parseEther("1.0"));
```

## Run the tests

```bash
npx hardhat test        # Run all the tests
npx hardhat test --grep "something"     # Run tests containing "something" in their descriptions
npx hardhat test --bail     # Stop running tests after the first test failure
npx hardhat test --parallel
```

# How to define a new network for hardhat

1. Add a new network to `hardhat.config.js` inside `networks` property:

```
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

```
module.exports = {
  solidity: "0.8.9",
  defaultNetwork: "ganache",
...
```

Alternatively, It's possible to run the tests with `--network` option:

```bash
npx hardhat test --network ganache
```

# How to debug

- Use `--log-jsonrpc` option to enable Json-RPC requests/responses logging. It only works with ethers.js currently.
- Use `hre.logDebug`
- Use vscode debugger
- Use `--verbose` option to enable hardhat verbose logging.

```bash
npx hardhat --verbose test
```

- Use `--debug` to print out log messages.

```bash
npx hardhat --debug test
```

# Testing conventions and best practices

- File names tries to tell us the scenario we're testing.
- We don't pollute test results with logs. So if you want to add them for debugging, please consider using `--debug` flag and use `hre.logDebug` function:

```javascript
hre.logDebug(result);
```

- Every `it` block should have one assertion, readable and easy to understand.
- Follow `GIVEN`, `WHEN` and, `THEN` BDD style
  - Top level `describe` for `GIVEN`
  - Nested `describe`s for `WHEN`
  - `it` blocks for `THEN`

```javascript
// GIVEN
describe("Contract with payable constructor", function () {
  // WHEN
  describe("When ethers.js is used", function () {
    let contract;
    let INITIAL_BALANCE = 10;

    before(async function () {
      const Contract = await ethers.getContractFactory("WithPayableConstructor");
      contract = await Contract.deploy({
        value: INITIAL_BALANCE
      });
    });

    // THEN
    it("Should be deployed successfully", async function () {
      expect(contract.address).exist;
    });
  });
});
```

- It's acceptable to disable tests as long as the following rules are fulfilled:
  1. A useless test should be removed from code, not disabled.
  2. A disabled test should be in `xit` instead of `it` block. `xit` blocks are for skipping tests. Commented tests are FORBIDDEN.
  3. A disabled test should have a `FIXME` comment containing an issue number to track it. Disabled tests must be addressed ASAP.

```javascript
    // FIXME: In ZIL-4879
    xit("Should not be possible to move more than available tokens to some address", async function () {
```

- We use `[@tag1, @tag2, @tag3, ...]` in test descriptions to add tags to tests. This is based on [Mocha's tagging convention](https://github.com/mochajs/mocha/wiki/Tagging). In order to run `tag1` tests, you can use `--grep @tag1`.

```javascript
it("Should return correct value for string [@transactional, @ethers_js]", async function () {
  await contract.setName(STRING);
  expect(await contract.getStringPublic()).to.be.eq(STRING);
});
```

- `@transactional` tag is used for those tests which generate ethereum transactions. Calling pure functions or view functions doesn't generate a transaction for example. Transactional tests may use for populating an empty testnet with some transactions.

# miscellaneous

## Setup github pre-commit hook

You may want to set up pre-commit hook to fix your code before commit by:
`npm run prepare`

Alternatively, you can always fix your code manually before uploading to remote:

`npm run lint`

## Feed devnet with transactions

It's possible to use [FeedDevnet.js](scripts/FeedDevnet.js) to send transactions to devnet continuously:

```bash
npx hardhat run scripts/FeedDevnet.js --network devnet
```

Instead of `devnet` we can pass any other networks in the [config file](hardhat.config.js).
