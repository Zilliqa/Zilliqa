import {deploy, ScillaContract} from "../../helper/ScillaHelper";
import {expect} from "chai";
import {getAddressFromPrivateKey} from "@zilliqa-js/zilliqa";

describe("Scilla SetGet contract", function () {
  let contract: ScillaContract;
  before(async function () {
    const privateKey = "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004";
    const address = getAddressFromPrivateKey(privateKey);
    contract = await deploy("HelloWorld", address);
  });

  it("Should be deployed successfully", async function () {
    expect(contract.address).to.be.properAddress;
  });

  it("Can be possible to call setHello by the owner", async function () {
    const tx = await contract.setHello("salam");
    expect(tx).to.sendEventWithParams("setHello()", {value: "2", vname: "code"});
    expect(await contract.welcome_msg()).to.be.eq("salam");
  });

  it("Should send getHello() event when getHello() transition is called", async function () {
    const tx = await contract.getHello();
    expect(tx).to.sendEventWithParams("getHello()", {value: "salam", vname: "msg"});
  });
});
