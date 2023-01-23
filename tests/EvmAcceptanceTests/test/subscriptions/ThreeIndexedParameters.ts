import deepEqualInAnyOrder from "deep-equal-in-any-order";
import chai from "chai";

chai.use(deepEqualInAnyOrder);

import {expect} from "chai";
import {Contract, BigNumber} from "ethers";
import hre, {ethers} from "hardhat";
import {parallelizer} from "../../helpers";
import {Event, waitForEvents} from "./shared";

describe("Subscriptions functionality", function () {
  let contract: Contract;
  let eventsContract: Contract;
  let senderAddress: string;
  before(async function () {
    contract = await parallelizer.deployContract("Subscriptions");
    senderAddress = await contract.signer.getAddress();
  });

  beforeEach(async function () {
    const provider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
    eventsContract = new ethers.Contract(contract.address, contract.interface, provider);
  });

  afterEach(async function () {
    eventsContract.removeAllListeners();
  });

  describe("When event is triggered with three indexed parameters", function () {
    it("Should receive event when no arguments are provided", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event3();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS: Event[] = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)},
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event3(event.to, event.amount))
          .to.emit(contract, "Event3")
          .withArgs(event.from, event.to, event.amount);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.deep.equalInAnyOrder(event);
      }
    });

    it("Should receive event when first argument is provided", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event3(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when second argument is provided", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87";
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when third argument is provided", async function () {
      let receivedEvents: Event[] = [];
      const FUNDS = ethers.BigNumber.from(300);
      const filter = eventsContract.filters.Event3(null, null, FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [{from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: FUNDS}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for third argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_FUNDS = [ethers.BigNumber.from(300), ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(null, null, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: FILTER_FUNDS[0]}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for first and second arguments", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for first and third arguments", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const FILTER_FUNDS = [ethers.BigNumber.from(300), ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES, null, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive event when 'or' filter is provided for second and third arguments", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const FILTER_FUNDS = [ethers.BigNumber.from(300), ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(300)}
      ];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.deep.equalInAnyOrder(event);
    });

    it("Should receive no events when wrong filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = "0xE5f1fF64fd5dB9113B05f4C17F23A0E92BF3b33E";
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for third argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";
      const filter = eventsContract.filters.Event3(null, null, ethers.BigNumber.from(200));
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for first argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = [
        "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
        "0xE5f1fF64fd5dB9113B05f4C17F23A0E92BF3b33E"
      ];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for second argument", async function () {
      let receivedEvents: Event[] = [];
      const FILTER_ADDRESSES = ["0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", senderAddress];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for third argument", async function () {
      let receivedEvents: Event[] = [];
      const FUND_FILTERS = [ethers.BigNumber.from(200), ethers.BigNumber.from(300)];
      const filter = eventsContract.filters.Event3(null, null, FUND_FILTERS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [{from: senderAddress, to: RECIPIENT, amount: ethers.BigNumber.from(100)}];

      const event: Event = VECTORS[0];
      await expect(contract.event3(event.to, event.amount))
        .to.emit(contract, "Event3")
        .withArgs(event.from, event.to, event.amount);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive event with complex scenario - 1", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event3(
        senderAddress,
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        ethers.BigNumber.from(300)
      );
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS: Event[] = [
        {
          from: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
          to: "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          amount: ethers.BigNumber.from(100)
        },
        {
          from: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
          to: "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          amount: ethers.BigNumber.from(200)
        },
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}, // captured
        {
          from: "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          to: "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          amount: ethers.BigNumber.from(400)
        }
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event3(event.to, event.amount)).to.emit(contract, "Event3");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.have.length(1);
    });

    it("Should receive event with complex scenario - 2", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event3(senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", [
        ethers.BigNumber.from(300),
        ethers.BigNumber.from(200)
      ]);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {
          from: "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
          amount: ethers.BigNumber.from(100)
        },
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(200)}, // captured
        {from: senderAddress, to: "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", amount: ethers.BigNumber.from(300)}, // captured
        {
          from: "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          to: "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          amount: ethers.BigNumber.from(400)
        }
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event3(event.to, event.amount)).to.emit(contract, "Event3");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.have.length(2);
    });
  });
});
