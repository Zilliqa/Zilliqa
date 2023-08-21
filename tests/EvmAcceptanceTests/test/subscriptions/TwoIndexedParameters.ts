import deepEqualInAnyOrder from "deep-equal-in-any-order";
import chai from "chai";

chai.use(deepEqualInAnyOrder);

import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";
import {Event, waitForEvents} from "./shared";

describe("Subscriptions functionality", function () {
  let contract: Contract;
  let eventsContract: Contract;
  let senderAddress: string;
  before(async function () {
    contract = await hre.deployContract("Subscriptions");
    senderAddress = await contract.signer.getAddress();
  });

  beforeEach(async function () {
    const provider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
    eventsContract = new ethers.Contract(contract.address, contract.interface, provider);
  });

  afterEach(async function () {
    eventsContract.removeAllListeners();
  });

  describe("When event is triggered with two indexed parameters", function () {
    it("Should receive event when no arguments are provided", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event2();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)}
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event2(event.to, event.amount))
          .to.emit(contract, "Event2")
          .withArgs(event.from, event.to, event.amount);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.deep.equalInAnyOrder(event);
      }
    });

    it("Should receive event when filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event2(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const filter = eventsContract.filters.Event2(null, RECIPIENT);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive no events when wrong 'or' filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c"
      ];
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = ["0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", senderAddress];
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399";
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = senderAddress;
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event2(event.to, event.amount))
        .to.emit(contract, "Event2")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });
  });
});
