import {SignerWithAddress} from "@nomiclabs/hardhat-ethers/signers";
import {Account} from "@zilliqa-js/account";

export default class SignerPool {
  public takeEthSigner(): SignerWithAddress {
    if (this.eth_signers.length == 0) {
      throw new Error(
        "No more signers to return. Either you haven't initialized this pool, or you just ran out of signers."
      );
    }

    return this.eth_signers.pop()!;
  }

  public takeZilSigner(): Account {
    if (this.zil_signers.length == 0) {
      throw new Error(
        "No more signers to return. Either you haven't initialized this pool, or you just ran out of signers."
      );
    }

    return this.zil_signers.pop()!;
  }

  public initSigners(signer: SignerWithAddress[], privateKeys: string[]) {
    this.releaseEthSigner(...signer);

    this.zil_signers.push(...privateKeys.map((key) => new Account(key)));
  }

  public releaseEthSigner(...signer: SignerWithAddress[]) {
    this.eth_signers.push(...signer);
  }

  public releaseZilSigner(...signer: Account[]) {
    this.zil_signers.push(...signer);
  }

  public getZilSigner(index: number): Account {
    return this.zil_signers[index];
  }

  public count(): [eth_count: number, zil_count: number] {
    return [this.eth_signers.length, this.zil_signers.length];
  }

  private eth_signers: SignerWithAddress[] = [];
  private zil_signers: Account[] = [];
}
