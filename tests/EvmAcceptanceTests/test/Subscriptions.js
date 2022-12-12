const {expect} = require("chai");
const {ethers} = require("hardhat");

const general_helper = require("../helper/GeneralHelper");

describe("Subscriptions functionality", function () {
  let contract;
  let eventsContract;
  let provider;
  let senderAddress;
  before(async function () {
    const Contract = await ethers.getContractFactory("Subscriptions");
    senderAddress = (await ethers.getSigner()).address;
    contract = await Contract.deploy();
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

  describe("When event is triggered with three indexed parameters", function () {
    it("Should receive event when no arguments are provided", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event3();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)],
        [senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
        receivedEvents = await waitForEvents(receivedEvents);
        expect(receivedEvents[i]).to.have.deep.members(VECTORS[i]);
      }
    });

    it("Should receive event when first argument is provided", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event3(senderAddress);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when second argument is provided", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87";
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when third argument is provided", async function () {
      let receivedEvents = [];
      const FUNDS = new ethers.BigNumber.from(300);
      const filter = eventsContract.filters.Event3(null, null, FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", FUNDS]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for third argument", async function () {
      let receivedEvents = [];
      const FILTER_FUNDS = [new ethers.BigNumber.from(300), new ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(null, null, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", FILTER_FUNDS[0]]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for first and second arguments", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for first and third arguments", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const FILTER_FUNDS = [new ethers.BigNumber.from(300), new ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES, null, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive event when 'or' filter is provided for second and third arguments", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e"];
      const FILTER_FUNDS = [new ethers.BigNumber.from(300), new ethers.BigNumber.from(500)];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES, FILTER_FUNDS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [[senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(300)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents[0]).to.have.deep.members(VECTORS[0]);
    });

    it("Should receive no events when wrong filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = "0xE5f1fF64fd5dB9113B05f4C17F23A0E92BF3b33E";
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong filter is provided for third argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e";
      const filter = eventsContract.filters.Event3(null, null, new ethers.BigNumber.from(200));
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for first argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = [
        "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
        "0xE5f1fF64fd5dB9113B05f4C17F23A0E92BF3b33E"
      ];
      const filter = eventsContract.filters.Event3(FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for second argument", async function () {
      let receivedEvents = [];
      const FILTER_ADDRESSES = ["0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", senderAddress];
      const filter = eventsContract.filters.Event3(null, FILTER_ADDRESSES);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive no events when wrong 'or' filter is provided for third argument", async function () {
      let receivedEvents = [];
      const FUND_FILTERS = [new ethers.BigNumber.from(200), new ethers.BigNumber.from(300)];
      const filter = eventsContract.filters.Event3(null, null, FUND_FILTERS);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const RECIPIENT = "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c";
      const VECTORS = [[senderAddress, RECIPIENT, new ethers.BigNumber.from(100)]];

      const [SENDER, DEST, AMOUNT] = VECTORS[0];
      await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3").withArgs(SENDER, DEST, AMOUNT);
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.be.empty;
    });

    it("Should receive event with complex scenario - 1", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event3(
        senderAddress,
        "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
        new ethers.BigNumber.from(300)
      );
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [
          "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c",
          "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          new ethers.BigNumber.from(100)
        ],
        [
          "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
          "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          new ethers.BigNumber.from(200)
        ],
        [senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)], // captured
        [
          "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          new ethers.BigNumber.from(400)
        ]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [_SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.have.length(1);
    });

    it("Should receive event with complex scenario - 2", async function () {
      let receivedEvents = [];
      const filter = eventsContract.filters.Event3(senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", [
        new ethers.BigNumber.from(300),
        new ethers.BigNumber.from(200)
      ]);
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [
          "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87",
          new ethers.BigNumber.from(100)
        ],
        [senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(200)], // captured
        [senderAddress, "0x6e2Cf2789c5B705E0990C05Ca959B5001c70BA87", new ethers.BigNumber.from(300)], // captured
        [
          "0x2a2ce8aFd5AfBBFb76B720D5e6048EFb056177E5",
          "0x9d1F9D4D70a35d18797E2495a8F73B9C8A08E399",
          new ethers.BigNumber.from(400)
        ]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [_SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event3(DEST, AMOUNT)).to.emit(contract, "Event3");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      expect(receivedEvents).to.have.length(2);
    });
  });

  describe("When two subscribers listen to events", function () {
    it("Should receive an event comming only from contract it is subscribed to", async function () {
      const Contract = await ethers.getContractFactory("Subscriptions");
      const secondContract = await Contract.deploy();
      const secondProvider = new ethers.providers.WebSocketProvider(general_helper.getWebsocketUrl());
      const secondEventsContract = new ethers.Contract(
        secondContract.address,
        secondContract.interface,
        secondProvider
      );

      let receivedEvents = [];
      let secondContractReceivedEvents = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [_SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event0(DEST, AMOUNT)).to.emit(contract, "Event0");
        await expect(secondContract.event0(DEST, AMOUNT)).to.emit(secondContract, "Event0");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(2);
    });
  });
  describe("When two subscribers listen to events", function () {
    it("Should deliver event to both", async function () {
      const secondEventsContract = new ethers.Contract(contract.address, contract.interface, provider);

      let receivedEvents = [];
      let secondContractReceivedEvents = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push([from, to, amount]);
      });

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [_SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event0(DEST, AMOUNT)).to.emit(contract, "Event0");
      }

      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(2);
    });
    it("Should deliver event to only a valid one", async function () {
      const secondEventsContract = new ethers.Contract(contract.address, contract.interface, provider);

      let receivedEvents = [];
      let secondContractReceivedEvents = [];

      const filter = eventsContract.filters.Event0();
      eventsContract.on(filter, (from, to, amount, _event) => {
        receivedEvents.push([from, to, amount]);
      });

      const filter2 = secondEventsContract.filters.Event0();
      secondEventsContract.on(filter2, (from, to, amount, _event) => {
        secondContractReceivedEvents.push([from, to, amount]);
      });

      // Deliberately close subscription
      secondEventsContract.removeAllListeners();

      const VECTORS = [
        [senderAddress, "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", new ethers.BigNumber.from(100)],
        [senderAddress, "0xF0Cb24aC66ba7375Bf9B9C4Fa91E208D9EAAbd2e", new ethers.BigNumber.from(200)]
      ];

      for (let i = 0; i < VECTORS.length; ++i) {
        const [_SENDER, DEST, AMOUNT] = VECTORS[i];
        await expect(contract.event0(DEST, AMOUNT)).to.emit(contract, "Event0");
      }
      receivedEvents = await waitForEvents(receivedEvents);
      secondContractReceivedEvents = await waitForEvents(secondContractReceivedEvents);
      expect(receivedEvents).to.have.length(2);
      expect(secondContractReceivedEvents).to.have.length(0);
    });
  });
});
