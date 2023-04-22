const {assert, expect} = require("chai");
import {ScillaContract} from "hardhat-scilla-plugin";
import {parallelizer} from "../../helpers";

describe("Ecdsa contract", () => {
  let contract: ScillaContract;
  it("Deploy ecdsa contract", async () => {
    contract = await parallelizer.deployScillaContract("Ecdsa");
    assert.isTrue(contract.address !== undefined);
  });

  it("recover invalid input and failed", async () => {
    const tx = await contract.recover(
      "0x1beedbe103d0b0da3f0ff6f8b614569c92174fb82e04c6676f9aa94b994774c5",
      "0x5f2afac816d9430bce53e081667378790bb5b703ea4a98234649ccac8a358f7a262553d9df46d8417239138bc69db8c458620093b2124937c6a5af2d86f0014e",
      32
    );

    expect(tx.receipt.success).equal(false);
    assert.include(tx.receipt.exceptions[0].message, "Sign.read_recoverable_exn: recid must be 0, 1, 2 or 3");
  });

  it("recover valid input and failed", async () => {
    const tx = await contract.recover(
      "0x1beedbe103d0b0da3f0ff6f8b614569c92174fb82e04c6676f9aa94b994774c5",
      "0x5f2afac816d9430bce53e081667378790bb5b703ea4a98234649ccac8a358f7a262553d9df46d8417239138bc69db8c458620093b2124937c6a5af2d86f0014e",
      2
    );

    expect(tx.receipt.success).equal(false);
    assert.include(tx.receipt.exceptions[0].message, "Sign.recover: pk could not be recovered");
  });
});
