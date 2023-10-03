# Quick Start

```bash
    npm install
    npx hardhat setup   # to run setup wizard
    npx hardhat test    # to run tests
    npx hardhat test --parallel   # to run tests in parallel mode
    npx hardhat test --network devnet    # to run tests against the devnet
    npx hardhat test --log-jsonrpc    # to run tests and print JSON-RPC requests/responses
    npx hardhat test --log-txnid    # to run tests and print transaction ids.
    DEBUG=true npx hardhat test    # to run tests and print log messages. `.env` file can be used as well.
    npx hardhat test --grep something    # to run tests containing `something` in the description
    npx hardhat test filename    # to run tests of `filename`
    npx hardhat test folder/*    # to run tests of `folder`
    npx hardhat test test/scilla/*    # to run scilla tests only
    SCILLA=false npx hardhat test   # to disable scilla tests. `.env` file can be used as well.
    MOCHA_TIMEOUT=300000 npx hardhat test   # To increase mocha timeout.
    MOCHA_REPORTER=MOCHA_REPORTER=./BriefMochaReporter.ts npx hardhat test    # To use brief reporter.
```

# Start running tests

If it's the first time you want to run tests, or you just created [a new network](#how-to-define-a-new-network-for-hardhat) and you want to run tests against this newly added network, you're supposed to call `setup`:

```bash
npx hardhat setup --network <network_name> #If --network is not specified, the default one will be used. In our config, the default network is isolated_server
```

During setup, a dozen of signers (accounts) will be created for you and will be saved to `.signers` folder. For every network you run setup for, a new `<network_name>.json` file is created in the `.signers` folder. This signers will be used for future test runs.

After the setup, run:

```bash
npx hardhat test --network <network_name>
```

This will run tests in sequential mode. _It takes more time to finish_ but _has more test cases_. It's also possible to run tests in parallel using:

```bash
npx hardhat test --parallel --network <network_name>
```

More faster but with fewer test cases.

# Start writing new tests

## A few simple rules before start

1. Please prefer ethers.js library to web3.js. Our default library to use throughout the the code is **ethers.js**.
2. Please use [typescript](./Typescript.md). Javascript is not used anymore in this test suite. You can learn more about typescript [here](./Typescript.md).
3. Please don't add commented tests. You can't add disabled tests as well, unless you create a ticket for it.

For more info, see [Testing conventions and best practices](#testing-conventions-and-best-practices).

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

## Tag tests to run in parallel mode

By default, test scenarios will be executed in the sequential mode, but if you want to make them run in parallel mode as well:

1. Add `#parallel` tag to its `describe` description.

```typescript
describe("Blockchain Instructions contract #parallel", function () {
```

2. Specify the block number that `it` block (real test code) is supposed to run in like `@block-1`. Although most of the tests should be executed in @block-1,it t could be executed in 1, 2, 3,... . But the more depth is added to our tests, the more time we need to wait for the results.

```typescript
  it("Should be deployed successfully @block-1", async function () {
```

## Run the tests

```bash
npx hardhat test        # Run all the tests
npx hardhat test --grep "something"     # Run tests containing "something" in their descriptions
npx hardhat test --parallel
npx hardhat test --bail     # Stop running tests after the first test failure
```

### Change mocha reporter

Mocha uses a reporter to print test results. It has some [predefined reporters](https://www.codingninjas.com/studio/library/mocha-reporters). In order to change the reporter, set `MOCHA_REPORTER` to one of those like `dot` or use our customized one by setting `MOCHA_REPORTER=./BriefMochaReporter.ts`. This reporter just prints timed out tests and failures. It also prints the failure call stack as soon as the test fails.

# How to define a new network for hardhat

1. Add a new network to `hardhat.config.ts` inside `networks` property:

```javascript
...
const config: any = {
  solidity: "0.8.9",
  defaultNetwork: "isolated_server",
  networks: {
    ganache: {
      url: "http://127.0.0.1:7545",
      chainId: 1337,
      accounts: [
        "c95690aed4461afd835b17492ff889af72267a8bdf7d781e305576cd8f7eb182",
        "05751249685e856287c2b2b9346e70a70e1d750bc69a35cef740f409ad0264ad",
        ...loadFromSignersFile("network_name")
      ]
    },
...
```

Don't forget to add `...loadFromSignersFile("network_name")` at the end of you accounts, otherwise your signers from `.signers` folder won't be loaded.

2. Change the default network:

```javascript
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

- Use `--log-txnid` to print out the transaction IDs.
- Use `--log-jsonrpc` option to enable Json-RPC requests/responses logging. It only works with ethers.js currently.
- Use vscode debugger
- Use `logDebug` and `DEBUG=true` environment variable to print out log messages.

```bash
DEBUG=true npx hardhat test
```

Alternatively you can change `DEBUG` variable in the `.env` file.

# Testing conventions and best practices

- File names tries to tell us the scenario we're testing.
- We don't pollute test results with logs. So if you want to add them for debugging, please consider using `logDebug` function:

```typescript
import {logDebug} from "../helpers";
logDebug(result);
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

- Second parameter to `expect` function is used to log in the case of test failure. We use it to debug failing tests on devnet or testnet easier.

```javascript
const txn = await payer.sendTransaction({
  to: payee.address,
  value: FUND
});

expect(await ethers.provider.getBalance(payee.address), `Txn Hash: ${txn.hash}`).to.be.eq(FUND);
```

# Scilla

## Testing

Scilla testing is done through the [hardhat scilla plugin](https://www.npmjs.com/package/hardhat-scilla-plugin). It's possible to deploy a scilla contract by its name and call its transitions just like a normal function call. It's also possible to get a field value through a function call. In the below sections, all of these topics are covered in detail.

### Deploy a contract

To deploy a contract all you need to know is its name:

```typescript
import {parallelizer} from "../../helpers";

let contract: ScillaContract = await parallelizer.deployScillaContract("SetGet");
let contract: ScillaContract = await parallelizer.deployScillaContract("HelloWorld", "Hello World"); // Contract with initial parameters.
```

### Call a transition

It's not harder than calling a normal function in typescript.
Let's assume we have a transition named `Set` which accepts a `number` as its parameter. Here is how to call it:

```typescript
await contract.Set(12);
```

### Get field value

If a given contract has a filed named `msg` is possible to get its current value using a function call to `msg()`

```typescript
const msg = await contract.msg();
```

### Expect a result

Chai matchers can be used to expect a value:

```typescript
it("Should set state correctly", async function () {
  const VALUE = 12;
  await contract.Set(VALUE);
  expect(await contract.value()).to.be.eq(VALUE);
});
```

There are two custom chai matchers specially developed to `expect` scilla events. `eventLog` and `eventLogWithParams`.
Use `eventLog` if you just need to expect event name:

```typescript
it("Should contain event data if emit function is called", async function () {
  const tx = await contract.emit();
  expect(tx).to.have.eventLog("Emit");
});
```

Otherwise, if you need to deeply expect an event, you should use `eventLogWithParams`. The first parameter is again the event name. The rest are parameters of the expected event. If you expect to have an event like `getHello` sending a parameter named `msg` with a `"hello world"` value:

```typescript
it("Should send getHello() event when getHello() transition is called", async function () {
  const tx = await contract.getHello();
  expect(tx).to.have.eventLogWithParams("getHello()", {value: "hello world", vname: "msg"});
});
```

You can even expect data type of the parameter(s):

```typescript
expect(tx).to.have.eventLogWithParams("getHello()", {value: "hello world", vname: "msg", type: "String"});
```

Type should be a valid Scilla type.

But if you just want to expect on the value of a event parameter do this:

```typescript
expect(tx).to.have.eventLogWithParams("getHello()", {value: "hello world"});
```

for more tests please take look at [scilla tests](./test/scilla/).

### TODO

- Support formatting complex data types such as `Map` and `List`.

## Tasks

### Scilla checker task

To run `scilla-checker` on all of the scilla contracts in the [contracts directory](./contracts/) run:

```bash
npx hardhat scilla-check --libdir path_to_stdlib
```

alternatively, you can check a specific file(s):

```bash
npx hardhat scilla-check --libdir path_to_stdlib contracts/scilla/helloWorld.scilla
```

### TODO

- Add `scilla-fmt` task

# miscellaneous

## .env File

to change some of the testing behaviors environment variables are used. They can be changed using the `.env` file. Here is the list of them:

- `DEBUG=true` to enable debugging logs.
- `SCILLA=false` to ignore scilla tests.
- `MOCHA_TIMEOUT=3000` to set the mocha timeout in milliseconds.

## Tasks

A few customized tasks are added to hardhat to simplify the process of test development and debugging.

### Balances

To get the balances of accounts in `hardhat.config.ts` this task can be used like:

```bash
npx hardhat balances --zil # returns balances of zil-addresses of accounts
npx hardhat balances --eth # returns balances of eth-addresses of accounts
npx hardhat balances  # same as --eth
```

### zilBalance

To get the balance of a zilliqa-based address of a private key, send that private key to `zilBalance` task:

```bash
npx hardhat zilBalance db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3
```

### transfer

To transfer fund from a private key (account) to a recipient, Use `transfer`. Because every private key in zilliqa network potentially can have two different addresses (one for zilliqa itself, and one for ethereum), you need to provide address type as well.

```bash
npx hardhat transfer --from d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba --to cf671756a8238cbeb19bcb4d77fc9091e2fce1a3 --amount 1000000 --address-type eth
```

`--from` is the private key of the sender.
`--from-address-type` can be either `eth` or `zil`. If `eth` is used, Funds are transferred from eth-based address of the private key. Otherwise, funds are transferred from zil-based address of the private key.
`--to` is the address of the recipient.
`--amount` is amount to be transferred in ZIL/ETH ZIL unit.

Example:

```bash
npx hardhat transfer --from db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3 --to 6e2cf2789c5b705e0990c05ca959b5001c70ba87 --amount 1 --from-address-type zil
```

## Scripts

To get the balances of the current accounts, run:

```bash
npx hardhat run scripts/Accounts.ts
npx hardhat run scripts/Accounts.ts --network public_testnet
```

When you start a testnet, your funds are initially in zil addresses, which is inconvenient.
The following script takes the private keys you have in your hardhat config,
and moves half of the funds at that address to the same address, but eth style.

```bash
npx hardhat run scripts/FundAccountsFromZil.ts --network testnet
```

Similarly you can do the same job if your initial accounts have funds in their eth addresses:
```bash
npx hardhat run scripts/FundAccountsFromEth.ts --network your_network
```

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

Instead of `devnet` we can pass any other networks defined in the [config file](hardhat.config.js).

## Increase tests timeout

Set the timeout as a environment variable before running the tests. It's in milliseconds.

```bash
MOCHA_TIMEOUT=300000 npx hardhat test
```

## Testing a newly deployed testnet/devent

_You need to have one prefunded account(private key)._ Let's call it PRIMARY_ACCOUNT.

1. Create a new hardhat network using the devent URL. Please refer to [How to define a new network for hardhat](#how-to-define-a-new-network-for-hardhat) for more info. But be sure to provide at least four different private keys for `accounts` in the config. These accounts will be funded by your primary prefunded account. So it doesn't matter if they all have zero balances.

2. Run `PRIMARY_ACCOUNT=YOUR_PRIVATE_KEY npx hardhat run scripts/FundAccountsFromPrimaryAccount.ts --network your_new_network`
   You should see something like this output:

```
Private key: db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3
Address: 0x7bb3B0E8A59f3f61d9Bff038f4AEb42cAE2ECce8
Balance: 999998799988000000
0xF0cB24aC66ba7375bF9B9C4fa91E208d9eAABD2E funded.
0xCf671756a8238cbeb19bcB4d77fC9091e2FCE1A3 funded.
0x05a321d0b9541ca08D7E32315CA186cc67A1602C funded.
0x6E2cf2789C5B705E0990C05cA959b5001C70bA87 funded.
```

It means that now all of your accounts specified in the `hardhat.config.ts` have enough funds to run tests.

3. Run tests using `npx hardhat test --network your_new_network` or any other variants of it.
