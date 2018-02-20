import { Component, Input, OnInit } from '@angular/core';

import { Wallet } from '../wallet/wallet'
import { ZilliqaService } from '../zilliqa.service';


@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.css']
})
export class HomeComponent implements OnInit {

  /* STATES:
   * 0: start view - create or access wallet
   * 1: create wallet - click to generate
   * 2: create wallet - successfully generated
   * 3: import wallet - show options
   * 4: import wallet - import json
   * 5: import wallet - enter key
   * 6: import wallet - successfully imported
   * 7: import wallet - invalid private key
   */
  state: number
  generatedPrivateKey: string
  uploadedWallet: File = null
  wallet: Wallet
  
  @Input() importPrivateKey = ''

  constructor(private zilliqaService: ZilliqaService) { 
    this.state = 0;
    this.wallet = new Wallet()
  }

  ngOnInit() { }

  setState(newState) {
    if (newState == 0) {
      // reset all variables
      this.wallet = new Wallet()
      this.generatedPrivateKey = '';
    }
    this.state = newState;
  }

  generateWallet() {
    this.generatedPrivateKey = this.zilliqaService.createWallet().privateKey
    this.wallet = this.zilliqaService.getWallet()
    this.setState(2);
  }

  importWallet() {
    if (this.zilliqaService.importWallet(this.importPrivateKey))
      this.setState(6)
    else
      this.setState(7)
  }

  selectWalletFile(files: FileList) {
    this.uploadedWallet = files.item(0)
    let reader = new FileReader()
    reader.onload = function(event) {
      let contents = event.target['result']
      console.log(contents)
    }

    reader.readAsText(this.uploadedWallet)
  }

  uploadWallet() {
    // this.zilliqaService.parseWallet(this.uploadWallet).then(function(data) {
    // })
  }
}
