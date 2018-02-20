import { Component, Input, OnInit, OnDestroy } from '@angular/core';

import { Wallet } from '../wallet'
import { ZilliqaService } from '../../zilliqa.service';


@Component({
  selector: 'app-wallet-send',
  templateUrl: './WalletSend.component.html',
  styleUrls: ['./WalletSend.component.css']
})

export class WalletSendComponent implements OnInit {

  /* STATES:
   * 0: send payment - input
   * 1: send payment - pending transaction
   */
  state: number
  pendingTxId: string
  wallet: Wallet

	@Input() payment = {}

  constructor(private zilliqaService: ZilliqaService) { 
    this.state = 0
    this.pendingTxId = null
    this.wallet = new Wallet()
    this.payment = {
      amount: 0,
      address: '',
      gasPrice: 0,
      gasLimit: 0
    }
  }

  ngOnInit() {
    this.wallet = this.zilliqaService.getWallet()
  }

  ngOnDestroy() {
    this.payment = {}
    this.pendingTxId = null
    this.setState(0)
  }

  setState(newState) {
    this.state = newState
  }

  onSend() {
    // if (this.payment.amount > this.wallet.balance) {
    //   alert('Amount must be within wallet balance.')
    //   return
    // }
    let that = this
    this.zilliqaService.sendPayment(this.payment).then(function(data) {
      that.pendingTxId = data.txId
      that.setState(1)
    })
  }

}
