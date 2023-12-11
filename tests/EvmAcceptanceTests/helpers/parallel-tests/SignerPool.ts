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
    // Let's shuffle signers to use them evenly
    this.releaseEthSigner(...shuffleArray(signer));

    this.zil_signers.push(...shuffleArray(privateKeys).map((key) => new Account(key)));
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

function shuffleArray(array: any[]) {
  for (let i = array.length - 1; i > 0; i--) {
    var j = Math.floor(Math.random() * (i + 1));
    var temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }

  return array;
}
