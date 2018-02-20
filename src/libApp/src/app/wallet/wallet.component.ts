import { Component, OnInit, OnDestroy } from '@angular/core';

import { ZilliqaService } from '../zilliqa.service';


@Component({
  selector: 'app-wallet',
  templateUrl: './wallet.component.html',
  styleUrls: ['./wallet.component.css']
})
export class WalletComponent implements OnInit {

  constructor(private zilliqaService: ZilliqaService) { 
  }

  ngOnInit() {
  }

  ngOnDestroy() {
  	this.zilliqaService.resetWallet()
  }

}
