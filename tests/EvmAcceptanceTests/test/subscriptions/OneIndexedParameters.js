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

  describe("When event is triggered with one indexed parameter", function () {
    it("Should receive event when single argument for filter is provided", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event1(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event1(DEST, AMOUNT)).to.emit(contract, "Event1").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESS = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";

      const filter = eventsContract.filters.Event1([senderAddress, FILTER_ADDRESS]);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event1(DEST, AMOUNT)).to.emit(contract, "Event1").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive no events when incorrect filter is provided", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESS = "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87";

      const filter = eventsContract.filters.Event1(FILTER_ADDRESS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event1(DEST, AMOUNT)).to.emit(contract, "Event1").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when incorrect 'or' filter is provided", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c"
      ];

      const filter = eventsContract.filters.Event1(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event1(DEST, AMOUNT)).to.emit(contract, "Event1").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive event when 'or' filter is provided with many operands", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];

      const filter = eventsContract.filters.Event1(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event1(DEST, AMOUNT)).to.emit(contract, "Event1").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });
  });
});
