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

  describe("When event is triggered with two indexed parameters", function () {
    it("Should receive event when no arguments are provided", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event2();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.have.deep.members(VECTORS[i]);
      }
    });

    it("Should receive event when filter is provided for first argument", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event2(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when filter is provided for second argument", async function () {
      let receivedEvents = [];
      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const filter = eventsContract.filters.Event2(null, RECIPIENT);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
        senderAddress
      ];
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive no events when wrong 'or' filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c"
      ];
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = ["0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", senderAddress];
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399";
      const filter = eventsContract.filters.Event2(FILTER_ADDRESSES, null);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = senderAddress;
      const filter = eventsContract.filters.Event2(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event2(DEST, AMOUNT)).to.emit(contract, "Event2").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });
  });
});
