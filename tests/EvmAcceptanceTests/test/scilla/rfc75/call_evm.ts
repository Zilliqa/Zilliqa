import hre from "hardhat";
import {expect} from "chai";

describe("RFC75 Scilla to EVM functionality", function () {
  before(async function () {
    if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
      this.skip();
    }
  });

  it("Should be possible to...", async function () {
    expect(true).to.be.true;
  });
});
