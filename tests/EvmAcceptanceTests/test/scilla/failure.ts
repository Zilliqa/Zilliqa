import {ScillaContract} from "hardhat-scilla-plugin";
import hre from "hardhat";
import { expect } from "chai";
import { Zilliqa } from "@zilliqa-js/zilliqa";
import { bytes, Long, units } from "@zilliqa-js/util";

describe("Scilla Contract Deployment", function () {
  let contract: ScillaContract;
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }
  });

  it("Deployment should fail", async function () {
    const zilliqa = new Zilliqa("https://dev-api.zilliqa.com")
    zilliqa.wallet.addByPrivateKey("254d9924fc1dcdca44ce92d80255c6a0bb6123123123123e626fbfef4d357004");
    const contract = zilliqa.contracts.new(
      "(* HelloWorld contract *)\n\n(***************************************************)\n(*                 Scilla version                  *)\n(***************************************************)\n\nscilla_version 0\n\n(***************************************************)\n(*               Associated library                *)\n(***************************************************)\nlibrary HelloWorld\n\nlet not_owner_code  = Uint32 1\nlet set_hello_code  = Uint32 2\n\n(***************************************************)\n(*             The contract definition             *)\n(***************************************************)\n\ncontract HelloWorld\n(owner: ByStr20)\n\nfield welcome_msg : String = \"\"\n\ntransition setHello (msg : String)\n  is_owner = builtin eq owner _sender;\n  match is_owner with\n  | False =>\n    e = {_eventname : \"setHello\"; code : not_owner_code};\n    event e\n  | True =>\n    welcome_msg := msg;\n    e = {_eventname : \"setHello\"; code : set_hello_code};\n    event e\n  end\nend\n\ntransition getHello ()\n  r <- welcome_msg;\n  e = {_eventname: \"getHello\"; msg: r};\n  event e\nend\n",
      [ { vname: "owner", type: "ByStr20", value: "BE9F088A82107D1637bFD139682F4F38836bFc51" }, { vname: "_scilla_version", type: "Uint32", value: "0" }],
    );
    const [tx, _] = await contract.deploy(
      {
        version: bytes.pack(333, 1),
        gasPrice: units.toQa("2000", units.Units.Li),
        gasLimit: Long.fromNumber(25000),
      }
    );

    expect(tx.getReceipt()?.success).to.be.false
  });
});
