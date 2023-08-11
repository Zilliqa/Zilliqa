import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";

export default class SignerPool {
  public takeSigner(): SignerWithAddress {
    if (this.signers.length == 0) {
      throw new Error("No more signers to return. Try to add more signers to config");
    }

    return this.signers.pop()!;
  }

  public initSigners(...signer: SignerWithAddress[]) {
    this.releaseSigner(...signer);
  }

  public releaseSigner(...signer: SignerWithAddress[]) {
    this.signers.push(...signer);
  }

  private signers: SignerWithAddress[] = [];
}
