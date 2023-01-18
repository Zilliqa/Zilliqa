import deepEqualInAnyOrder from "deep-equal-in-any-order";
import chai from "chai";

chai.use(deepEqualInAnyOrder);

import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";
import {parallelizer} from "../../helpers";
import {Event, waitForEvents} from "./shared";

describe("Subscriptions functionality", function () {
  let contract: Contract;
  let eventsContract: Contract;
  let provider;
  let senderAddress: string;
  before(async function () {
    contract = await parallelizer.deployContract("Subscriptions");
    senderAddress = await contract.signer.getAddress();
  });

  beforeEach(async function () {
    provider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
    eventsContract = new ethers.Contract(contract.address, contract.interface, provider);
  });

  afterEach(async function () {
    eventsContract.removeAllListeners();
  });

  describe("When event is triggered with one indexed parameter", function () {
    it("Should receive event when single argument for filter is provided", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event1(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event1(event.to, event.amount))
        .to.emit(contract, "Event1")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESS = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";

      const filter = eventsContract.filters.Event1([senderAddress, FILTER_ADDRESS]);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS: Event[] = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event1(event.to, event.amount))
        .to.emit(contract, "Event1")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive no events when incorrect filter is provided", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESS = "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87";

      const filter = eventsContract.filters.Event1(FILTER_ADDRESS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS: Event[] = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event1(event.to, event.amount))
        .to.emit(contract, "Event1")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when incorrect 'or' filter is provided", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c"
      ];

      const filter = eventsContract.filters.Event1(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event1(event.to, event.amount))
        .to.emit(contract, "Event1")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive event when 'or' filter is provided with many operands", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];

      const filter = eventsContract.filters.Event1(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event1(event.to, event.amount))
        .to.emit(contract, "Event1")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(VECTORS[0]);
    });
  });
});
