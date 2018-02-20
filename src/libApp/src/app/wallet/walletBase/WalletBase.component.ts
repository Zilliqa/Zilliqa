import { Component, OnInit } from '@angular/core';

import { Wallet } from '../wallet'
import { ZilliqaService } from '../../zilliqa.service';


@Component({
  selector: 'app-wallet-base',
  templateUrl: './WalletBase.component.html',
  styleUrls: ['./WalletBase.component.css']
})
export class WalletBaseComponent implements OnInit {

  privateKey: string
	wallet: Wallet

  constructor(private zilliqaService: ZilliqaService) {
  	this.wallet = new Wallet()
    this.privateKey = '**************************'
  }

  ngOnInit() {
    this.wallet = this.zilliqaService.getWallet()
  }

  revealPrivateKey() {
    if (this.privateKey == undefined || this.privateKey.length == 0 || this.privateKey[0] != '*')
      // if privateKey is uninitialized or empty or doesn't begin with *, hide it
      this.privateKey = '**************************'
    else
      this.privateKey = this.wallet.address
  }

  downloadWallet() {
    let text = this.zilliqaService.generateWalletJson('testPassphrase')

    let element = document.createElement('a');
    element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
    element.setAttribute('download', 'wallet.json');

    element.style.display = 'none';
    document.body.appendChild(element);

    element.click();

    document.body.removeChild(element);
  }
}
