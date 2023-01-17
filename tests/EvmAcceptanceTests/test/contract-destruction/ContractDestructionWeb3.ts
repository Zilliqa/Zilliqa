import {expect} from "chai";
import {parallelizer} from "../../helpers";
import hre, {web3} from "hardhat";
import {Contract} from "web3-eth-contract";
import BN from "bn.js";

describe("Contract destruction with web3.js", function () {
  let amountPaid: BN;
  before(function () {
    amountPaid = web3.utils.toBN(web3.utils.toWei("3", "gwei"));
  });

  describe("When a user method call", function () {
    let contract: Contract;
    before(async function () {
      contract = await parallelizer.deployContractWeb3("ParentContract", {value: amountPaid});
    });

    it("should be destructed and coins in the contract should be transferred to the address specified in the method [@transactional]", async function () {
      expect(await contract.methods.getPaidValue().call()).to.be.eq(amountPaid);
      const destAccount = web3.eth.accounts.create().address;
      const prevBalance = await web3.eth.getBalance(destAccount);

      expect(await contract.methods.returnToSenderAndDestruct(destAccount).send()).to.be.not.null;
      const newBalance = await web3.eth.getBalance(destAccount);

      // Dest Account should have prevBalance + amountPaid
      expect(web3.utils.toBN(newBalance)).to.be.equal(web3.utils.toBN(prevBalance).add(amountPaid));
    });
  });

  describe("When a method call happens through another contract", function () {
    let contract: Contract;
    before(async function () {
      contract = await parallelizer.deployContractWeb3("ParentContract", {value: amountPaid});
    });

    it("Should be destructed and coins in the contract should be transferred to the address specified in the method [@transactional]", async function () {
      const result = await contract.methods.installChild(123).send({gasLimit: 1000000});
      expect(result).to.be.not.null;

      const childAddress = await contract.methods.childAddress().call();
      const childContract = new web3.eth.Contract(hre.artifacts.readArtifactSync("ChildContract").abi, childAddress, {
        from: contract.options.from,
        gas: contract.options.gas
      });

      const prevBalance = await web3.eth.getBalance(contract.options.address);
      await childContract.methods.returnToSender().send();
      const newBalance = await web3.eth.getBalance(contract.options.address);
      // Parent contract should have prevBalance + amountPaid
      expect(web3.utils.toBN(newBalance)).to.be.equal(web3.utils.toBN(prevBalance).add(amountPaid));
    });
  });
});
