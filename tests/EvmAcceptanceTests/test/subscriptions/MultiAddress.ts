import {expect} from "chai";
import {Contract} from "ethers";
import hre, {ethers, Web3} from "hardhat";
import {Log} from "web3-core/types";

describe("Subscriptions functionality", function () {
  let contract: Contract;
  let senderAddress: string;

  let web3 = new Web3(hre.getWebsocketUrl());

  before(async function () {
    contract = await hre.deployContract("Subscriptions");
    senderAddress = await contract.signer.getAddress();
  });

  describe("When a subscription is created with a singleton address list", function () {
    it("Should receive events for the address", async function () {
      let receivedEvents: Log[] = [];
      web3.eth.subscribe("logs", {address: [contract.address]}, (error, log) => {
        if (error) {
          throw error;
        }
        receivedEvents.push(log);
      });

      let event = {to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)};
      await expect(contract.event0(event.to, event.amount)).to.emit(contract, "Event0");

      await new Promise((r) => setTimeout(r, 5000));
      expect(receivedEvents).to.have.length(1);
      expect(
        receivedEvents.every((e) => {
          return e.address === contract.address;
        })
      ).to.be.equal(true);
    });
  });

  describe("When a subscription is created with two addresses", function () {
    it("Should receive events for both addresses", async function () {
      const secondContract = await hre.deployContract("Subscriptions");

      let receivedEvents: Log[] = [];
      web3.eth.subscribe("logs", {address: [contract.address, secondContract.address]}, (error, log) => {
        if (error) {
          throw error;
        }
        receivedEvents.push(log);
      });

      let event = {to: "0x05A321d0B9541Ca08d7e32315Ca186cC67A1602c", amount: ethers.BigNumber.from(100)};
      await expect(contract.event0(event.to, event.amount)).to.emit(contract, "Event0");
      await expect(secondContract.event0(event.to, event.amount)).to.emit(secondContract, "Event0");

      await new Promise((r) => setTimeout(r, 5000));
      expect(receivedEvents).to.have.length(2);
      expect(
        receivedEvents.every((e) => {
          return e.address === contract.address || e.address === secondContract.address;
        })
      ).to.be.equal(true);
    });
  });
});
