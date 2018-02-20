import { Component, OnInit } from '@angular/core';

import { Wallet } from '../wallet'
import { ZilliqaService } from '../../zilliqa.service';


@Component({
  selector: 'app-wallet-history',
  templateUrl: './WalletHistory.component.html',
  styleUrls: ['./WalletHistory.component.css']
})
export class WalletHistoryComponent implements OnInit {

	txHistory = []
  wallet: Wallet

  constructor(private zilliqaService: ZilliqaService) { 
  	this.wallet = new Wallet()
    this.txHistory = []
  }

  ngOnInit() {
  	this.wallet = this.zilliqaService.getWallet()
    let that = this
    this.zilliqaService.getTxHistory().then(function(data) {
      that.txHistory = data
    })
  }

}
