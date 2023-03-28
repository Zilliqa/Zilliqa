import { expect } from "chai";
import {ethers} from "hardhat";

type Deployment = {
  contractName: string;
  arguments: any[];
};

const NUMBER = 100;
const NAME = "Zilliqa";
const ADDRESS = "0x71C7656EC7ab88b098defB751B7401B5f6d8976F";
const ENUM = 1;
const INITIAL_BALANCE = 10;
const VALUE = 1_000_000;


async function deployContracts(...contracts: Deployment[]) {
  let promises: Promise<any>[] = [];
  let signers = await ethers.getSigners();
  for (let i = 0; i < contracts.length; ++i) {
    const deployment = contracts[i];
    const signer = signers[i];
    let contract = await ethers.getContractFactory(deployment.contractName);
    promises.push(contract.connect(signer).deploy(...deployment.arguments));
  }

  return await Promise.all(promises);
}

type Expected = {
  promise: Promise<any>,
  msg?: string,
  expected: any
}

async function expectAll(rawPromises: Promise<any>[],expeditions: Expected[]) {
  console.log(rawPromises);
  await Promise.all(rawPromises);
  const promises = expeditions.map((expectation: Expected) => expectation.promise);
  const values = await Promise.all(promises);

  for (let i = 0; i < expeditions.length; ++i) {
    const expectation = expeditions[i];
    expect(values[i]).to.be.eq(expectation.expected);
  }
}

async function main() {
  let [
    zeroParamContract,
    withUintContract,
    withStringContract,
    withAddressContract,
    withEnumContract,
    multiParamContract,
    withPayableContract,
    delegateContract,
    testDelegateContract
  ]
     = await deployContracts(
    {contractName: "ZeroParamConstructor", arguments: []},
    {contractName: "WithUintConstructor", arguments: [NUMBER]},
    {contractName: "WithStringConstructor", arguments: [NAME]},
    {contractName: "WithAddressConstructor", arguments: [ADDRESS]},
    {contractName: "WithEnumConstructor", arguments: [ENUM]},
    {contractName: "MultiParamConstructor", arguments: [NAME, NUMBER]},
    {contractName: "WithPayableConstructor", arguments: [{value: INITIAL_BALANCE}]},
    {contractName: "Delegatecall", arguments: []},
    {contractName: "TestDelegatecall", arguments: []},

  );

  expect(zeroParamContract.address).to.be.properAddress;
  expect(withUintContract.address).to.be.properAddress;
  expect(withStringContract.address).to.be.properAddress;
  expect(withAddressContract.address).to.be.properAddress;
  expect(withEnumContract.address).to.be.a.properAddress;
  expect(multiParamContract.address).to.be.properAddress;
  expect(withPayableContract.address).to.be.properAddress;

  await delegateContract.setVars(testDelegateContract.address, NUMBER, {value: VALUE})
  await expectAll(
    [
      //delegateContract.setVars(testDelegateContract.address, NUMBER, {value: VALUE})
    ],
    [
    {promise: zeroParamContract.number(), expected: 123},
    {promise: withUintContract.number(), expected: NUMBER},
    {promise: withStringContract.name(), expected: NAME},
    {promise: withAddressContract.someAddress(), expected: ADDRESS},
    {promise: withEnumContract.someEnum(), expected: ENUM},
    {promise: multiParamContract.number(), expected: NUMBER},
    {promise: multiParamContract.name(), expected: NAME},
    {promise: withPayableContract.balance(), expected: INITIAL_BALANCE},
    {promise: withPayableContract.owner(), expected: withPayableContract.signer.address},
    {promise: delegateContract.num(), expected: 100},
    // {promise: delegateContract.value(), expected: VALUE},
    // {promise: delegateContract.sender(), expected: delegateContract.signer},
    // {promise: ethers.provider.getBalance(delegateContract.address), expected: VALUE},
    // {promise: testDelegateContract.num(), expected: 0},
    // {promise: testDelegateContract.value(), expected: 0},
    // {promise: testDelegateContract.sender(), expected: "0x0000000000000000000000000000000000000000"},
    // {promise: ethers.provider.getBalance(testDelegateContract.address), expected: 0}
  ])
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
