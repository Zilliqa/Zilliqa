import {expect} from "chai";
import {Contract, utils} from "ethers";
import hre, {ethers} from "hardhat";
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../helpers";
import {defaultAbiCoder, toUtf8Bytes} from "ethers/lib/utils";
import {Event} from "./subscriptions/shared";
import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";

function validateScillaEvent(scillaEventName: string, contractAddress: string, event: any) {
  expect(event["address"].toLowerCase()).to.eq(contractAddress.toLowerCase());
  const EXPECTED_TOPIC_0 = utils.keccak256(toUtf8Bytes(scillaEventName + "(string)"));
  expect(event["topics"][0].toLowerCase()).to.eq(EXPECTED_TOPIC_0.toLowerCase());
  const decodedData = defaultAbiCoder.decode(["string"], event["data"]);
  const scillaEvent = JSON.parse(decodedData.toString());
  expect(scillaEvent["_eventname"]).to.be.equal(scillaEventName);
}

function validateEvmEvent(evmEventName: string, contractAddress: string, event: any) {
  expect(event["address"].toLowerCase()).to.eq(contractAddress.toLowerCase());
  const EXPECTED_TOPIC_0 = utils.keccak256(toUtf8Bytes(evmEventName + "()"));
  expect(event["topics"][0].toLowerCase()).to.eq(EXPECTED_TOPIC_0.toLowerCase());
}

describe("ERC20 Is ZRC2", function () {
  let zrc2_contract: ScillaContract;
  let erc20_contract: Contract;
  let erc165_contract: Contract;
  let contractOwner: SignerWithAddress;
  let alice: SignerWithAddress;
  let bob: SignerWithAddress;

  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }

    contractOwner = hre.allocateEthSigner();

    zrc2_contract = await parallelizer.deployScillaContract(
      "FungibleToken",
      await contractOwner.getAddress(),
      "ERC20isZRC2 Token",
      "SDT",
      2,
      1_000
    );
    alice = hre.allocateEthSigner();
    bob = hre.allocateEthSigner();
    erc20_contract = await hre.deployContractWithSigner(
      "ERC20isZRC2",
      contractOwner,
      zrc2_contract.address?.toLowerCase()
    );

    erc165_contract = await hre.deployContractWithSigner("ContractSupportingScillaReceiver", contractOwner);
  });

  after(() => {
    hre.releaseEthSigner(alice, bob, contractOwner);
  });

  it("Interop Should be deployed successfully", async function () {
    expect(zrc2_contract.address).to.be.properAddress;
    expect(erc20_contract.address).to.be.properAddress;
    expect(erc165_contract.address).to.be.properAddress;
  });

  it("should return correct contract owner from ZRC2", async function () {
    expect(await zrc2_contract.contract_owner()).to.be.eq(await contractOwner.getAddress());
  });

  it("Should return zrc2/erc20 total supply via bridge contract", async function () {
    expect(await erc20_contract.totalSupply()).to.be.eq(1_000);
  });

  it("Should return init supply via bridge contract", async function () {
    expect(await erc20_contract.initSupply()).to.be.eq(1_000);
  });

  it("Should return token name via bridge contract", async function () {
    expect(await erc20_contract.tokenName()).to.be.eq("ERC20isZRC2 Token");
  });

  it("Should be able to transfer via erc20", async function () {
    let receipt = await erc20_contract.transfer(await alice.getAddress(), 150);
    receipt = await receipt.wait();
    const zrc2Tokens = await erc20_contract.balanceOf(await alice.getAddress());
    expect(zrc2Tokens).to.be.eq(150);
    // Validate receipt events
    {
      const events = receipt["events"];
      validateScillaEvent("TransferSuccess", zrc2_contract.address!, events[0]);
      validateEvmEvent("TransferEvent", erc20_contract.address, events[1]);
    }
  });

  it("Should be able properly manipulate allowances", async function () {
    // Allow 50
    let receipt = await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50);
    receipt = await receipt.wait();
    // Validate receipt
    {
      const events = receipt["events"];
      validateScillaEvent("IncreasedAllowance", zrc2_contract.address!, events[0]);
      validateEvmEvent("IncreasedAllowanceEvent", erc20_contract.address, events[1]);
    }
    let aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(50);

    // Allow 50 more
    receipt = await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 100);
    receipt = await receipt.wait();
    {
      const events = receipt["events"];
      validateScillaEvent("IncreasedAllowance", zrc2_contract.address!, events[0]);
      validateEvmEvent("IncreasedAllowanceEvent", erc20_contract.address, events[1]);
    }

    aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(100);

    // Allow 50 fewer
    receipt = await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50);
    receipt = await receipt.wait();
    {
      const events = receipt["events"];
      validateScillaEvent("DecreasedAllowance", zrc2_contract.address!, events[0]);
      validateEvmEvent("DecreasedAllowanceEvent", erc20_contract.address, events[1]);
    }

    aliceAllowance = await erc20_contract.allowance(await alice.getAddress(), await contractOwner.getAddress());
    expect(aliceAllowance).to.be.eq(50);
  });

  it("Should be able to transferFrom via erc20", async function () {
    await (await erc20_contract.connect(alice).approve(await contractOwner.getAddress(), 50)).wait();
    await (
      await erc20_contract.connect(contractOwner).transferFrom(await alice.getAddress(), await bob.getAddress(), 50)
    ).wait();
    const aliceTokens = await erc20_contract.balanceOf(await alice.getAddress());
    expect(aliceTokens).to.be.eq(100);
    const bobTokens = await erc20_contract.balanceOf(await bob.getAddress());
    expect(bobTokens).to.be.eq(50);
  });

  it("Should be able to transfer to evm contract", async function () {
    await (await erc20_contract.connect(contractOwner).transfer(erc20_contract.address, 150)).wait();
    const zrc2Tokens = await erc20_contract.balanceOf(erc20_contract.address);
    expect(zrc2Tokens).to.be.eq(150);
  });

  it("Should not be able to transfer to evm contract when _EvmCall tag is present", async function () {
    expect(erc20_contract.connect(contractOwner).transferFailed(erc20_contract.address, 150)).to.be.reverted;
    const zrc2Tokens = await erc20_contract.balanceOf(erc20_contract.address);
    expect(zrc2Tokens).to.be.eq(150);
  });

  it("Should be able to receive scilla event via subscriptions", async function () {
    const provider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
    let receivedEvents: Event[] = [];
    const filter = {
      address: zrc2_contract.address?.toLowerCase(),
      topics: [utils.id("TransferSuccess(string)")]
    };
    provider.on(filter, (event) => {
      receivedEvents.push(event);
    });

    let receipt = await erc20_contract.transfer(await alice.getAddress(), 150);
    await receipt.wait();
    await new Promise((r) => setTimeout(r, 2000));
    expect(receivedEvents).to.be.not.empty;

    const queriedLogs = await erc20_contract.queryFilter(filter);
    expect(
      queriedLogs.every((e) => {
        return e["data"].startsWith("0x");
      })
    ).to.be.equal(true);
  });

  it("Should not be able to transfer to evm contract when scilla receiver handler is present", async function () {
    const scillaSignature = utils.id("handle_scilla_message(string,bytes)").slice(0, 10);
    expect(await erc165_contract.supportsInterface(scillaSignature)).to.be.true;
    expect(erc20_contract.transfer(erc165_contract.address, 150)).to.be.reverted;
    const zrc2Tokens = await erc20_contract.balanceOf(erc165_contract.address);
    expect(zrc2Tokens).to.be.eq(0);
  });
});
