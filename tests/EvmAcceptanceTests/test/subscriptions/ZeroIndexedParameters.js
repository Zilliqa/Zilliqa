const {expect} = require("chai");
const {ethers} = require("hardhat");

const general_helper = require("../../helper/GeneralHelper");
const parallelizer = require("../../helper/Parallelizer");

describe("Subscriptions functionality", function () {
  let contract;
  let eventsContract;
  let provider;
  let senderAddress;
  before(async function () {
    contract = await parallelizer.deployContract("Subscriptions");
    senderAddress = contract.signer.address;
  });

  beforeEach(async function () {
    provider = new ethers.providers.WebSocketProvider(general_helper.getWebsocketUrl());
    eventsContract = new ethers.Contract(contract.address, contract.interface, provider);
  });

  afterEach(async function () {
    eventsContract.removeAllListeners();
  });

  // Ensures all events in current eventloop run are dispatched
  async function waitForEvents(events, timeout = 5000) {
    await new Promise((r) => setTimeout(r, timeout));
    return events;
  }

  describe("When event is triggered with zero indexed parameters", function () {
    it("Should receive event regardless of provided filters", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event0(DEST, AMOUNT)).to.emit(contract, "Event0").withArgs(SENDER, DEST, AMOUNT);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.have.deep.members(VECTORS[i]);
      }
    });
  });
});
