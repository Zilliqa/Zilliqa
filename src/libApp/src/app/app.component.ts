import { Component } from '@angular/core';

import { ZilliqaService } from './zilliqa.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent {

	data = {}

	constructor(private zilliqaService: ZilliqaService) { 
		this.data = {
			latestDSBlock: '',
			networkId: ''
		}
	}

	ngOnInit() {
		let that = this;
		this.zilliqaService.getInitData().then(function(data) {
			that.data = data;
		});
	}
}
