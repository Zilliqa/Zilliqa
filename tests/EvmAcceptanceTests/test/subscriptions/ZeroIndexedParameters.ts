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

  describe("When event is triggered with zero indexed parameters", function () {
    it("Should receive event regardless of provided filters", async function () {
      let receivedEvents: Event[] = [];
      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push({from, to, amount});
      });

      const VECTORS = [
        {from: senderAddress, to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)},
        {from: senderAddress, to: "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", amount: ethers.BigNumber.from(200)}
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const event: Event = VECTORS[i];
        await expect(contract.event0(event.to, event.amount))
          .to.emit(contract, "Event0")
          .withArgs(event.from, event.to, event.amount);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.deep.equalInAnyOrder(event);
      }
    });
  });
});
