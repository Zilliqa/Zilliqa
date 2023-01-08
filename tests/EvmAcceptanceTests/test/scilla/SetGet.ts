/* deploy the SetGet contract and basic interactions   */

import {deploy_from_file} from "../../helper/ScillaHelper";

// async function run() {
//   let tx = (sc = null);
//   try {
//     // deploy the contract
//     const init = [{vname: "_scilla_version", type: "Uint32", value: "0"}];
//     [tx, sc] = await deploy_from_file("contracts/scilla/SetGet.scilla", init);
//     console.log("contract deployed @ ", sc.address);
//     try {
//       // call set(.) and log state
//       const val = 16;
//       const args = [{vname: "v", type: "Uint128", value: val.toString()}];
//       tx = await sc_call(sc, "Set", args);
//       const state = await sc.getState();
//       console.log(`state after call to Set(${val}):`, state);
//       try {
//         // call emit() and log event emitted
//         tx = await sc_call(sc, "Emit");
//         console.log("event_logs[0] after call to Emit():\n", tx.receipt.event_logs[0]);
//       } catch (err) {
//         console.log("Emit(): ERROR\n", err);
//       }
//     } catch (err) {
//       console.log("Set(.): ERROR\n", err);
//     }
//   } catch (err) {
//     console.log("deploy_from_file(.): ERROR\n", err);
//   }
// }

import {expect} from "chai";

describe("Scilla SetGet contract", function () {
  before(async function () {
    const init = [{vname: "_scilla_version", type: "Uint32", value: "0"}];
    let sc = await deploy_from_file("contracts/scilla/SetGet.scilla", init);
    this.contract = sc;
  });

  it("Should be deployed successfully", async function() {
    expect(this.contract.address).to.be.properAddress;
  });

  it("Should set state correctly", async function() {
    const ret = await this.contract.Set(12);
    expect(ret.value).to.be.eq('12');
  });
});
