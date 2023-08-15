import deepEqualInAnyOrder from "deep-equal-in-any-order";
import chai from "chai";

chai.use(deepEqualInAnyOrder);

import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers} from "hardhat";
import {Event, waitForEvents} from "./shared";
import {WebSocketProvider} from "@ethersproject/providers";

describe("Subscriptions functionality", function () {
  let contract: Contract;
  let eventsContract: Contract;
  let senderAddress: string;
  let provider: WebSocketProvider;
  before(async function () {
    contract = await hre.deployContract("Subscriptions");
    senderAddress = await contract.signer.getAddress();
  });

  beforeEach(async function () {
    provider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
    eventsContract = new ethers.Contract(contract.address, contract.interface, provider);
  });

  afterEach(async function () {
    eventsContract.removeAllListeners();
  });

  describe("When two subscribers listen to events", function () {
    it("Should receive an event coming only from contract it is subscribed to", async function () {
      const secondContract = await hre.deployContract("Subscriptions");
      const secondProvider = new ethers.providers.WebSocketProvider(hre.getWebsocketUrl());
      const secondEventsContract = new ethers.Contract(
        secondContract.address,
        secondContract.interface,
        secondProvider
      );

      let receivedEvents: Event[] = [];
      let secondContractReceivedEvents: Event[] = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)}
      ];

      // Expect two events on the first contract and one on the second.
      await expect(contract.event0(VECTORS[0].to, VECTORS[0].amount)).to.emit(contract, "Event0");
      await expect(contract.event0(VECTORS[1].to, VECTORS[1].amount)).to.emit(contract, "Event0");
      await expect(secondContract.event0(VECTORS[0].to, VECTORS[0].amount)).to.emit(secondContract, "Event0");

      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(1);
    });
    it("Should deliver event to both", async function () {
      const secondEventsContract = new ethers.Contract(contract.address, contract.interface, provider);

      let receivedEvents: Event[] = [];
      let secondContractReceivedEvents: Event[] = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)}
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event0(event.to, event.amount)).to.emit(contract, "Event0");
      }

      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(2);
    });

    // FIXME: In https://zilliqa-jira.atlassian.net/browse/ZIL-5064
    xit("Should deliver event to only a valid one", async function () {
      const secondEventsContract = new ethers.Contract(contract.address, contract.interface, provider);

      let receivedEvents: Event[] = [];
      let secondContractReceivedEvents: Event[] = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push({from, to, amount});
      });

      // Deliberately close subscription
      secondEventsContract.removeAllListeners();

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)}
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event0(event.to, event.amount)).to.emit(contract, "Event0");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(0);
    });
  });
});
