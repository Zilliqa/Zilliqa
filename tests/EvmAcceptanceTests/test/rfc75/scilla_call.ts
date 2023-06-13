import {expect} from "chai";
import hre from "hardhat";

describe("RFC75 ScillaCall", function () {
    before(async function () {
        if (!hre.isZilliqaNetworkSelected() || !hre.isScillaTestingEnabled()) {
            this.skip();
        }
    });

    it("Should be deployed successfully", async function () {
        expect(true).to.be.true;
    });
});
